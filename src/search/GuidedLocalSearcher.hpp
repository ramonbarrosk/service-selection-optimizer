#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

#include "Allocation.h"
#include "InstanceMatrix.hpp"
#include "ProbabilityScenario.h"
#include "Service.h"
#include "SolutionValidator.hpp"
#include "Task.h"

// Opções separadas permitem medir a contribuição de cada melhoria da GLS/GFLS.
// A configuração padrão corresponde à versão promovida para o projeto.
struct GuidedLocalSearchOptions {
    bool useGfls = true;
    bool useIncrementalFeasibility = true;
    bool useServiceReplacementNeighborhood = true;
    // O fatorial 2^4 mostrou resultado melhor mantendo a escala clássica
    // calculada a partir da solução inicial; a calibração local permanece opcional.
    bool calibrateLambdaAfterInitialDescent = false;
    bool useRegretUtility = true;
    // Limite zero preserva a regra clássica: penalizar todos os empates de
    // utilidade máxima. A alternativa top-1 piorou o resultado agregado.
    int maximumPenalizedFeatures = 0;
    int probabilityRebuildInterval = 32;
    int deadlineCheckInterval = 128;
};

struct GuidedLocalSearchStats {
    long long evaluatedCandidates = 0;
    long long rejectedByCapacity = 0;
    long long rejectedByServiceLimit = 0;
    long long rejectedBySla = 0;
    long long acceptedMoves = 0;
    long long acceptedSwaps = 0;
    long long acceptedServiceReplacements = 0;
    long long completedPenaltyRounds = 0;
    double lambda = 0.0;
};

// Busca Local Guiada aplicada às atribuições tarefa--serviço.
//
// Cada atribuição (tarefa t no serviço j) é uma característica com penalidade
// p[t][j]. A descida usa:
//
//     h(s) = custoReal(s) + lambda * somaDasPenalidadesPresentes(s).
//
// Capacidade, Smax e SLA continuam sendo restrições duras. As penalidades apenas
// guiam a trajetória; o resultado é sempre a melhor solução viável pelo custo real.
class GuidedLocalSearcher {
    SolutionValidator validator_;
    std::vector<std::vector<int>> penalties_;
    std::vector<unsigned char> activeServiceNeighborhoods_;
    double lambda_ = 0.0;
    GuidedLocalSearchStats stats_;

    static double nowMs() {
        using namespace std::chrono;
        return static_cast<double>(duration_cast<milliseconds>(
            steady_clock::now().time_since_epoch()).count());
    }

    static bool deadlineReached(double deadlineMs) {
        return std::isfinite(deadlineMs) && nowMs() >= deadlineMs;
    }

    static std::vector<double> probabilityDistribution(
            const Allocation& allocation, const InstanceMatrix& matrix,
            int Vmax, int excludedTask = -1) {
        std::vector<double> distribution(Vmax + 1, 0.0);
        distribution[0] = 1.0;
        const std::vector<int>& assignedServices = allocation.getAllocation();
        for (int taskId = 0; taskId < matrix.getNumberOfTasks(); ++taskId) {
            if (taskId == excludedTask)
                continue;
            const int serviceId = assignedServices[taskId];
            if (serviceId < 0)
                continue;
            const double probability = matrix.getServiceProb(serviceId);
            for (int violations = Vmax; violations >= 0; --violations) {
                const double previous = violations > 0
                    ? distribution[violations - 1] : 0.0;
                distribution[violations] =
                    distribution[violations] * (1.0 - probability)
                    + previous * probability;
            }
        }
        return distribution;
    }

    static double violationProbability(const std::vector<double>& distribution) {
        return std::clamp(
            1.0 - std::accumulate(distribution.begin(), distribution.end(), 0.0),
            0.0, 1.0);
    }

    bool serviceNeighborhoodIsActive(int first, int second,
                                     const GuidedLocalSearchOptions& options) const {
        return !options.useGfls
            || activeServiceNeighborhoods_[first]
            || activeServiceNeighborhoods_[second];
    }

    void updateBest(const Allocation& candidate, Allocation& best) const {
        if (candidate.getCurrentCost() < best.getCurrentCost() - 1e-9)
            best = candidate;
    }

    bool moveRespectsCapacityAndServiceLimit(
            const Allocation& allocation, const InstanceMatrix& matrix,
            int taskId, int oldService, int newService, int Smax) {
        const int consumption = matrix.getTaskConsumption(taskId);
        const std::vector<int>& loads = allocation.getResourcePerService();
        if (loads[newService] + consumption > matrix.getVres()) {
            ++stats_.rejectedByCapacity;
            return false;
        }

        const std::vector<int>& counts = allocation.getTaskCountPerService();
        int employed = allocation.getNumberOfEmployedServices();
        if (counts[oldService] == 1)
            --employed;
        if (counts[newService] == 0)
            ++employed;
        if (employed > Smax) {
            ++stats_.rejectedByServiceLimit;
            return false;
        }
        return true;
    }

    bool swapRespectsCapacity(const Allocation& allocation,
                              const InstanceMatrix& matrix,
                              int task1, int service1,
                              int task2, int service2) {
        const std::vector<int>& loads = allocation.getResourcePerService();
        const int consumption1 = matrix.getTaskConsumption(task1);
        const int consumption2 = matrix.getTaskConsumption(task2);
        if (loads[service1] - consumption1 + consumption2 > matrix.getVres()
                || loads[service2] - consumption2 + consumption1 > matrix.getVres()) {
            ++stats_.rejectedByCapacity;
            return false;
        }
        return true;
    }

    // Executa uma descida de melhor melhoria em MOVE e SWAP. Quando a avaliação
    // incremental está ligada, a distribuição da SLA é atualizada em O(Vmax)
    // após um MOVE e não muda após SWAP.
    bool guidedDescent(Allocation& current, Allocation& best,
                       const InstanceMatrix& matrix,
                       int Vmax, int Smax, double Pmax,
                       ProbabilityScenario scenario, double deadlineMs,
                       const GuidedLocalSearchOptions& options) {
        const int numberOfTasks = matrix.getNumberOfTasks();
        const int numberOfServices = matrix.getNumberOfServices();
        bool appliedAnyMove = false;

        if (static_cast<int>(activeServiceNeighborhoods_.size()) != numberOfServices)
            activeServiceNeighborhoods_.assign(numberOfServices, 1);
        if (options.useGfls
                && std::none_of(activeServiceNeighborhoods_.begin(),
                                activeServiceNeighborhoods_.end(),
                                [](unsigned char active) { return active != 0; })) {
            return false;
        }

        std::vector<double> distribution;
        double currentViolation = 0.0;
        int movesSinceProbabilityRebuild = 0;
        if (options.useIncrementalFeasibility
                && scenario == ProbabilityScenario::Ps) {
            distribution = probabilityDistribution(current, matrix, Vmax);
            currentViolation = violationProbability(distribution);
        }

        long long candidatesSinceClockCheck = 0;
        const int clockBlock = std::max(1, options.deadlineCheckInterval);
        auto candidateTimedOut = [&]() {
            if (!options.useIncrementalFeasibility)
                return deadlineReached(deadlineMs);
            ++candidatesSinceClockCheck;
            if (candidatesSinceClockCheck < clockBlock)
                return false;
            candidatesSinceClockCheck = 0;
            return deadlineReached(deadlineMs);
        };

        while (!deadlineReached(deadlineMs)) {
            enum class MoveKind { NONE, MOVE, SWAP, SERVICE_REPLACEMENT };
            MoveKind bestKind = MoveKind::NONE;
            double bestDelta = -1e-9;
            double bestNewViolation = currentViolation;
            int bestTask1 = -1;
            int bestTask2 = -1;
            int bestService = -1;
            int bestSourceService = -1;
            bool timedOut = false;

            std::vector<std::vector<double>> distributionWithoutService(
                numberOfServices);
            std::vector<unsigned char> cachedWithoutService(numberOfServices, 0);
            auto prepareDistributionWithoutService =
                [&](int taskId, int serviceId) {
                    if (cachedWithoutService[serviceId])
                        return;
                    const double oldProbability = matrix.getServiceProb(serviceId);
                    const double denominator = 1.0 - oldProbability;
                    std::vector<double>& without =
                        distributionWithoutService[serviceId];
                    if (denominator > 1e-12) {
                        without.assign(Vmax + 1, 0.0);
                        without[0] = distribution[0] / denominator;
                        for (int violations = 1; violations <= Vmax; ++violations) {
                            without[violations] =
                                (distribution[violations]
                                 - oldProbability * without[violations - 1])
                                / denominator;
                        }
                    } else {
                        without = probabilityDistribution(
                            current, matrix, Vmax, taskId);
                    }
                    cachedWithoutService[serviceId] = 1;
                };

            for (int taskId = 0; taskId < numberOfTasks && !timedOut; ++taskId) {
                const int oldService = current.getServiceForTask(taskId);
                if (oldService < 0)
                    continue;
                for (int newService = 0; newService < numberOfServices; ++newService) {
                    ++stats_.evaluatedCandidates;
                    if (candidateTimedOut()) {
                        timedOut = true;
                        break;
                    }
                    if (newService == oldService
                            || !serviceNeighborhoodIsActive(
                                oldService, newService, options)) {
                        continue;
                    }

                    const double deltaCost =
                        matrix.getTaskCost(taskId, newService)
                        - matrix.getTaskCost(taskId, oldService);
                    const double deltaPenalty =
                        penalties_[taskId][newService]
                        - penalties_[taskId][oldService];
                    const double deltaGuided = deltaCost + lambda_ * deltaPenalty;
                    if (deltaGuided >= bestDelta)
                        continue;

                    bool feasible = false;
                    double candidateViolation = currentViolation;
                    if (options.useIncrementalFeasibility) {
                        if (!moveRespectsCapacityAndServiceLimit(
                                current, matrix, taskId, oldService,
                                newService, Smax)) {
                            continue;
                        }
                        if (scenario == ProbabilityScenario::Ps) {
                            prepareDistributionWithoutService(taskId, oldService);
                            candidateViolation = std::clamp(
                                currentViolation
                                + (matrix.getServiceProb(newService)
                                   - matrix.getServiceProb(oldService))
                                    * distributionWithoutService[oldService][Vmax],
                                0.0, 1.0);
                            feasible = candidateViolation <= Pmax + 1e-12;
                            if (!feasible)
                                ++stats_.rejectedBySla;
                        } else {
                            feasible = true;
                        }
                    } else {
                        Task task(taskId, matrix.getTaskConsumption(taskId));
                        current.replaceService(task, Service(newService), matrix);
                        feasible = validator_.isFeasible(
                            matrix, current, Vmax, Smax, Pmax, scenario, true);
                        current.replaceService(task, Service(oldService), matrix);
                    }

                    if (feasible && deltaGuided < bestDelta) {
                        bestDelta = deltaGuided;
                        bestKind = MoveKind::MOVE;
                        bestTask1 = taskId;
                        bestTask2 = -1;
                        bestService = newService;
                        bestNewViolation = candidateViolation;
                    }
                }
            }

            for (int task1 = 0; task1 < numberOfTasks && !timedOut; ++task1) {
                const int service1 = current.getServiceForTask(task1);
                if (service1 < 0)
                    continue;
                for (int task2 = task1 + 1; task2 < numberOfTasks; ++task2) {
                    ++stats_.evaluatedCandidates;
                    if (candidateTimedOut()) {
                        timedOut = true;
                        break;
                    }
                    const int service2 = current.getServiceForTask(task2);
                    if (service2 < 0 || service1 == service2
                            || !serviceNeighborhoodIsActive(
                                service1, service2, options)) {
                        continue;
                    }

                    const double deltaCost =
                        matrix.getTaskCost(task1, service2)
                        + matrix.getTaskCost(task2, service1)
                        - matrix.getTaskCost(task1, service1)
                        - matrix.getTaskCost(task2, service2);
                    const double deltaPenalty =
                        penalties_[task1][service2]
                        + penalties_[task2][service1]
                        - penalties_[task1][service1]
                        - penalties_[task2][service2];
                    const double deltaGuided = deltaCost + lambda_ * deltaPenalty;
                    if (deltaGuided >= bestDelta)
                        continue;

                    bool feasible = false;
                    if (options.useIncrementalFeasibility) {
                        // SWAP preserva o multiconjunto de probabilidades e Smax.
                        feasible = swapRespectsCapacity(
                            current, matrix, task1, service1, task2, service2);
                    } else {
                        Task first(task1, matrix.getTaskConsumption(task1));
                        Task second(task2, matrix.getTaskConsumption(task2));
                        current.replaceService(first, Service(service2), matrix);
                        current.replaceService(second, Service(service1), matrix);
                        feasible = validator_.isFeasible(
                            matrix, current, Vmax, Smax, Pmax, scenario, true);
                        current.replaceService(first, Service(service1), matrix);
                        current.replaceService(second, Service(service2), matrix);
                    }
                    if (feasible && deltaGuided < bestDelta) {
                        bestDelta = deltaGuided;
                        bestKind = MoveKind::SWAP;
                        bestTask1 = task1;
                        bestTask2 = task2;
                        bestService = -1;
                        bestNewViolation = currentViolation;
                    }
                }
            }

            // Esta vizinhança composta troca um serviço empregado por um serviço
            // vazio e transfere conjuntamente todas as suas tarefas. O número de
            // serviços empregados é preservado, permitindo atravessar barreiras
            // de Smax que MOVE e SWAP isolados não conseguem atravessar.
            if (options.useServiceReplacementNeighborhood && !timedOut) {
                const std::vector<int>& counts = current.getTaskCountPerService();
                for (int source = 0; source < numberOfServices && !timedOut; ++source) {
                    if (counts[source] == 0)
                        continue;
                    for (int target = 0; target < numberOfServices; ++target) {
                        ++stats_.evaluatedCandidates;
                        if (candidateTimedOut()) {
                            timedOut = true;
                            break;
                        }
                        if (counts[target] != 0 || source == target
                                || !serviceNeighborhoodIsActive(source, target, options)) {
                            continue;
                        }

                        double deltaCost = 0.0;
                        double deltaPenalty = 0.0;
                        std::vector<int> affectedTasks;
                        for (int taskId = 0; taskId < numberOfTasks; ++taskId) {
                            if (current.getServiceForTask(taskId) != source)
                                continue;
                            affectedTasks.push_back(taskId);
                            deltaCost += matrix.getTaskCost(taskId, target)
                                - matrix.getTaskCost(taskId, source);
                            deltaPenalty += penalties_[taskId][target]
                                - penalties_[taskId][source];
                        }
                        const double deltaGuided =
                            deltaCost + lambda_ * deltaPenalty;
                        if (deltaGuided >= bestDelta)
                            continue;

                        for (int taskId : affectedTasks) {
                            current.replaceService(
                                Task(taskId, matrix.getTaskConsumption(taskId)),
                                Service(target), matrix);
                        }
                        bool feasible = validator_.isFeasible(
                            matrix, current, Vmax, Smax, Pmax, scenario, true);
                        for (int taskId : affectedTasks) {
                            current.replaceService(
                                Task(taskId, matrix.getTaskConsumption(taskId)),
                                Service(source), matrix);
                        }
                        if (feasible) {
                            bestDelta = deltaGuided;
                            bestKind = MoveKind::SERVICE_REPLACEMENT;
                            bestSourceService = source;
                            bestService = target;
                            bestTask1 = -1;
                            bestTask2 = -1;
                        }
                    }
                }
            }

            if (timedOut)
                break;
            if (bestKind == MoveKind::NONE) {
                if (options.useGfls) {
                    std::fill(activeServiceNeighborhoods_.begin(),
                              activeServiceNeighborhoods_.end(), 0);
                }
                break;
            }

            if (bestKind == MoveKind::MOVE) {
                const int oldService = current.getServiceForTask(bestTask1);
                if (options.useIncrementalFeasibility
                        && scenario == ProbabilityScenario::Ps) {
                    prepareDistributionWithoutService(bestTask1, oldService);
                    const std::vector<double>& without =
                        distributionWithoutService[oldService];
                    const double newProbability = matrix.getServiceProb(bestService);
                    for (int violations = 0; violations <= Vmax; ++violations) {
                        distribution[violations] =
                            without[violations] * (1.0 - newProbability)
                            + (violations > 0
                                ? without[violations - 1] * newProbability : 0.0);
                    }
                    currentViolation = bestNewViolation;
                }
                current.replaceService(
                    Task(bestTask1, matrix.getTaskConsumption(bestTask1)),
                    Service(bestService), matrix);
                if (options.useGfls) {
                    activeServiceNeighborhoods_[oldService] = 1;
                    activeServiceNeighborhoods_[bestService] = 1;
                }
                ++stats_.acceptedMoves;

                if (options.useIncrementalFeasibility
                        && scenario == ProbabilityScenario::Ps
                        && ++movesSinceProbabilityRebuild
                            >= std::max(1, options.probabilityRebuildInterval)) {
                    distribution = probabilityDistribution(current, matrix, Vmax);
                    currentViolation = violationProbability(distribution);
                    movesSinceProbabilityRebuild = 0;
                }
            } else if (bestKind == MoveKind::SWAP) {
                const int service1 = current.getServiceForTask(bestTask1);
                const int service2 = current.getServiceForTask(bestTask2);
                current.replaceService(
                    Task(bestTask1, matrix.getTaskConsumption(bestTask1)),
                    Service(service2), matrix);
                current.replaceService(
                    Task(bestTask2, matrix.getTaskConsumption(bestTask2)),
                    Service(service1), matrix);
                if (options.useGfls) {
                    activeServiceNeighborhoods_[service1] = 1;
                    activeServiceNeighborhoods_[service2] = 1;
                }
                ++stats_.acceptedSwaps;
            } else {
                int movedTasks = 0;
                for (int taskId = 0; taskId < numberOfTasks; ++taskId) {
                    if (current.getServiceForTask(taskId) != bestSourceService)
                        continue;
                    current.replaceService(
                        Task(taskId, matrix.getTaskConsumption(taskId)),
                        Service(bestService), matrix);
                    ++movedTasks;
                }
                if (options.useGfls) {
                    activeServiceNeighborhoods_[bestSourceService] = 1;
                    activeServiceNeighborhoods_[bestService] = 1;
                }
                if (options.useIncrementalFeasibility
                        && scenario == ProbabilityScenario::Ps) {
                    distribution = probabilityDistribution(current, matrix, Vmax);
                    currentViolation = violationProbability(distribution);
                    movesSinceProbabilityRebuild = 0;
                }
                stats_.acceptedMoves += movedTasks;
                ++stats_.acceptedServiceReplacements;
            }

            appliedAnyMove = true;
            updateBest(current, best);
        }
        return appliedAnyMove;
    }

    void penalizeMaximumUtilityFeatures(
            const Allocation& localMinimum, const InstanceMatrix& matrix,
            const GuidedLocalSearchOptions& options) {
        struct Candidate {
            int taskId;
            double utility;
        };
        std::vector<Candidate> candidates;
        candidates.reserve(matrix.getNumberOfTasks());
        for (int taskId = 0; taskId < matrix.getNumberOfTasks(); ++taskId) {
            const int serviceId = localMinimum.getServiceForTask(taskId);
            if (serviceId < 0)
                continue;
            double featureCost = matrix.getTaskCost(taskId, serviceId);
            if (options.useRegretUtility) {
                featureCost = std::max(
                    0.0, featureCost - matrix.getMinCostForTask(taskId));
            }
            candidates.push_back({
                taskId,
                featureCost
                    / static_cast<double>(1 + penalties_[taskId][serviceId])
            });
        }
        std::stable_sort(candidates.begin(), candidates.end(),
            [](const Candidate& first, const Candidate& second) {
                return first.utility > second.utility;
            });
        if (candidates.empty())
            return;

        int selected = 0;
        const double maximumUtility = candidates.front().utility;
        for (const Candidate& candidate : candidates) {
            if (candidate.utility + 1e-12 < maximumUtility)
                break;
            if (options.maximumPenalizedFeatures > 0
                    && selected >= options.maximumPenalizedFeatures) {
                break;
            }
            const int serviceId =
                localMinimum.getServiceForTask(candidate.taskId);
            ++penalties_[candidate.taskId][serviceId];
            if (options.useGfls)
                activeServiceNeighborhoods_[serviceId] = 1;
            ++selected;
        }
    }

public:
    const GuidedLocalSearchStats& stats() const { return stats_; }

    Allocation improve(
            Allocation initial, const InstanceMatrix& matrix,
            int Vmax, int Smax, double Pmax, ProbabilityScenario scenario,
            double alpha = 0.3, int penaltyRounds = 30,
            bool preservePenalties = false,
            double deadlineMs = std::numeric_limits<double>::infinity(),
            const GuidedLocalSearchOptions& options = GuidedLocalSearchOptions{}) {
        const int numberOfTasks = matrix.getNumberOfTasks();
        const int numberOfServices = matrix.getNumberOfServices();
        const bool dimensionsChanged =
            static_cast<int>(penalties_.size()) != numberOfTasks
            || (!penalties_.empty()
                && static_cast<int>(penalties_.front().size()) != numberOfServices);
        const bool initializeState =
            !preservePenalties || dimensionsChanged || penalties_.empty();

        if (initializeState) {
            penalties_.assign(numberOfTasks, std::vector<int>(numberOfServices, 0));
            lambda_ = options.calibrateLambdaAfterInitialDescent
                ? 0.0
                : alpha * std::max(1.0, initial.getCurrentCost())
                    / std::max(1, numberOfTasks);
            stats_ = {};
        }
        activeServiceNeighborhoods_.assign(numberOfServices, 1);

        Allocation current = initial;
        Allocation best = initial;
        if (initializeState && options.calibrateLambdaAfterInitialDescent
                && !deadlineReached(deadlineMs)) {
            // Com penalidades zeradas, esta primeira descida minimiza apenas o
            // custo. O lambda passa a refletir a escala do mínimo local alcançado.
            guidedDescent(current, best, matrix, Vmax, Smax, Pmax, scenario,
                          deadlineMs, options);
            lambda_ = alpha * std::max(1.0, current.getCurrentCost())
                / std::max(1, numberOfTasks);
            activeServiceNeighborhoods_.assign(numberOfServices, 1);
        }

        for (int round = 0; round < penaltyRounds; ++round) {
            if (deadlineReached(deadlineMs))
                break;
            guidedDescent(current, best, matrix, Vmax, Smax, Pmax, scenario,
                          deadlineMs, options);
            if (deadlineReached(deadlineMs))
                break;
            penalizeMaximumUtilityFeatures(current, matrix, options);
            ++stats_.completedPenaltyRounds;
        }
        stats_.lambda = lambda_;
        return best;
    }
};
