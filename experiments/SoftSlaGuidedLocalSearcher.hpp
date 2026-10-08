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

struct SoftSlaSearchStats {
    long evaluatedCandidates = 0;
    long completedPenaltyRounds = 0;
    long evaluatedInfeasibleCandidates = 0;
    long acceptedInfeasibleMoves = 0;
    long feasibleRecoveries = 0;
    double maximumAcceptedExcess = 0.0;
    double finalProbabilityDrift = 0.0;
};

// Variante experimental da GLS em que somente a SLA probabilística é suave:
//
//   h_prob(s) = custo(s) + lambda * penalidades(s)
//               + mu * max(0, P_violacao(s) - Pmax).
//
// Capacidade e Smax permanecem restrições duras. A trajetória pode atravessar
// soluções que violam a SLA, mas o incumbente e a resposta final são sempre
// escolhidos entre as soluções viáveis pelo custo real.
class SoftSlaGuidedLocalSearcher {
    SolutionValidator validator_;
    std::vector<std::vector<int>> penalties_;
    std::vector<unsigned char> activeServiceNeighborhoods_;
    double lambda_ = 0.0;
    double probabilityPenalty_ = 0.0;
    double minimumProbabilityPenalty_ = 0.0;
    SoftSlaSearchStats stats_;

    static double nowMs() {
        using namespace std::chrono;
        return static_cast<double>(duration_cast<milliseconds>(
            steady_clock::now().time_since_epoch()).count());
    }

    static bool deadlineReached(double deadlineMs) {
        return std::isfinite(deadlineMs) && nowMs() >= deadlineMs;
    }

    bool moveRespectsHardConstraints(
            const Allocation& allocation, const InstanceMatrix& matrix,
            int taskId, int oldService, int newService, int Smax) const {
        const int consumption = matrix.getTaskConsumption(taskId);
        const std::vector<int>& loads = allocation.getResourcePerService();
        if (loads[newService] + consumption > matrix.getVres())
            return false;

        const std::vector<int>& counts = allocation.getTaskCountPerService();
        int employed = allocation.getNumberOfEmployedServices();
        if (counts[oldService] == 1)
            --employed;
        if (counts[newService] == 0)
            ++employed;
        return employed <= Smax;
    }

    bool swapRespectsHardConstraints(
            const Allocation& allocation, const InstanceMatrix& matrix,
            int task1, int service1, int task2, int service2) const {
        const std::vector<int>& loads = allocation.getResourcePerService();
        const int consumption1 = matrix.getTaskConsumption(task1);
        const int consumption2 = matrix.getTaskConsumption(task2);
        return loads[service1] - consumption1 + consumption2 <= matrix.getVres()
            && loads[service2] - consumption2 + consumption1 <= matrix.getVres();
    }

    double violationExcess(const Allocation& allocation,
                           const InstanceMatrix& matrix,
                           int Vmax, double Pmax,
                           ProbabilityScenario& scenario) {
        return validator_.computeViolationExcess(
            matrix, allocation, Vmax, Pmax, scenario);
    }

    // Distribui a probabilidade de ocorrerem 0, 1, ..., Vmax violações. Ela é
    // calculada no início, atualizada em O(Vmax) após MOVE e reutilizada após
    // SWAP e entre rodadas de penalização.
    std::vector<double> probabilityDistribution(
            const Allocation& allocation, const InstanceMatrix& matrix,
            int Vmax, int excludedTask = -1) const {
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
                const double withoutCurrent = violations > 0
                    ? distribution[violations - 1] : 0.0;
                distribution[violations] =
                    distribution[violations] * (1.0 - probability)
                    + withoutCurrent * probability;
            }
        }
        return distribution;
    }

    void updateBestFeasible(const Allocation& candidate, double excess,
                            Allocation& best) const {
        if (excess <= 1e-12
                && candidate.getCurrentCost() < best.getCurrentCost() - 1e-9)
            best = candidate;
    }

    bool guidedDescent(Allocation& current, Allocation& best,
                       double& currentExcess,
                       double& currentViolation,
                       std::vector<double>& distribution,
                       int& movesSinceProbabilityRebuild,
                       const InstanceMatrix& matrix,
                       int Vmax, int Smax, double Pmax,
                       ProbabilityScenario scenario, bool softSlaEnabled,
                       double deadlineMs) {
        const int numberOfTasks = matrix.getNumberOfTasks();
        const int numberOfServices = matrix.getNumberOfServices();
        bool appliedAnyMove = false;

        if (static_cast<int>(activeServiceNeighborhoods_.size()) != numberOfServices)
            activeServiceNeighborhoods_.assign(numberOfServices, 1);
        if (std::none_of(activeServiceNeighborhoods_.begin(),
                         activeServiceNeighborhoods_.end(),
                         [](unsigned char active) { return active != 0; }))
            return false;

        // Q guarda a distribuição sem uma tarefa do serviço indicado. Como a
        // probabilidade depende apenas do serviço, tarefas no mesmo serviço
        // compartilham o mesmo cache.
        std::vector<std::vector<double>> distributionWithoutService(
            numberOfServices);
        std::vector<unsigned char> cachedWithoutService(numberOfServices, 0);

        auto prepareDistributionWithoutService = [&](int taskId, int serviceId) {
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

        long candidatesSinceClockCheck = 0;
        auto checkDeadlineInBlock = [&]() {
            ++candidatesSinceClockCheck;
            if (candidatesSinceClockCheck < 128)
                return false;
            candidatesSinceClockCheck = 0;
            return deadlineReached(deadlineMs);
        };

        while (!deadlineReached(deadlineMs)) {
            enum class MoveKind { NONE, MOVE, SWAP };
            MoveKind bestKind = MoveKind::NONE;
            double bestDelta = -1e-9;
            double bestNewExcess = currentExcess;
            int bestTask1 = -1;
            int bestTask2 = -1;
            int bestService = -1;
            bool timedOut = false;

            // MOVE pode alterar tanto o custo e a penalidade GLS quanto a
            // probabilidade de violação. A solução temporária precisa respeitar
            // capacidade e Smax, mas a SLA pode ser excedida sob penalidade.
            for (int taskId = 0; taskId < numberOfTasks && !timedOut; ++taskId) {
                const int oldService = current.getServiceForTask(taskId);
                if (oldService < 0)
                    continue;

                for (int newService = 0; newService < numberOfServices; ++newService) {
                    ++stats_.evaluatedCandidates;
                    if (checkDeadlineInBlock()) {
                        timedOut = true;
                        break;
                    }
                    if (newService == oldService)
                        continue;
                    if (!activeServiceNeighborhoods_[oldService]
                            && !activeServiceNeighborhoods_[newService])
                        continue;

                    const double deltaCost =
                        matrix.getTaskCost(taskId, newService)
                        - matrix.getTaskCost(taskId, oldService);
                    const double deltaPenalty =
                        penalties_[taskId][newService]
                        - penalties_[taskId][oldService];
                    const double baseDelta = deltaCost + lambda_ * deltaPenalty;

                    // Partindo de uma solução viável, a parcela probabilística
                    // nunca pode melhorar o delta. Esse corte evita DPs inúteis.
                    if (currentExcess <= 1e-12 && baseDelta >= bestDelta)
                        continue;

                    if (!moveRespectsHardConstraints(
                            current, matrix, taskId, oldService,
                            newService, Smax)) {
                        continue;
                    }

                    double candidateExcess = currentExcess;
                    if (scenario == ProbabilityScenario::Ps) {
                        prepareDistributionWithoutService(taskId, oldService);
                        // Trocar p por p' altera diretamente a probabilidade de
                        // violação em (p' - p) * Q[Vmax].
                        const double candidateViolation = std::clamp(
                            currentViolation
                            + (matrix.getServiceProb(newService)
                               - matrix.getServiceProb(oldService))
                                * distributionWithoutService[oldService][Vmax],
                            0.0, 1.0);
                        candidateExcess = std::max(
                            0.0, candidateViolation - Pmax);
                    }
                    if (candidateExcess > 1e-12)
                        ++stats_.evaluatedInfeasibleCandidates;
                    if (!softSlaEnabled && candidateExcess > 1e-12)
                        continue;
                    const double deltaGuided = baseDelta
                        + (softSlaEnabled
                            ? probabilityPenalty_
                                * (candidateExcess - currentExcess)
                            : 0.0);

                    if (deltaGuided < bestDelta) {
                        bestDelta = deltaGuided;
                        bestKind = MoveKind::MOVE;
                        bestTask1 = taskId;
                        bestTask2 = -1;
                        bestService = newService;
                        bestNewExcess = candidateExcess;
                    }
                }
            }

            if (timedOut)
                break;

            // Um SWAP preserva o multiconjunto das probabilidades dos serviços;
            // portanto, o excesso de SLA não muda e seu delta probabilístico é zero.
            for (int task1 = 0; task1 < numberOfTasks && !timedOut; ++task1) {
                const int service1 = current.getServiceForTask(task1);
                if (service1 < 0)
                    continue;
                for (int task2 = task1 + 1; task2 < numberOfTasks; ++task2) {
                    ++stats_.evaluatedCandidates;
                    if (checkDeadlineInBlock()) {
                        timedOut = true;
                        break;
                    }
                    const int service2 = current.getServiceForTask(task2);
                    if (service2 < 0 || service1 == service2)
                        continue;
                    if (!activeServiceNeighborhoods_[service1]
                            && !activeServiceNeighborhoods_[service2])
                        continue;

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

                    if (swapRespectsHardConstraints(
                            current, matrix, task1, service1,
                            task2, service2)) {
                        bestDelta = deltaGuided;
                        bestKind = MoveKind::SWAP;
                        bestTask1 = task1;
                        bestTask2 = task2;
                        bestService = -1;
                        bestNewExcess = currentExcess;
                    }
                }
            }

            if (timedOut)
                break;
            if (bestKind == MoveKind::NONE) {
                std::fill(activeServiceNeighborhoods_.begin(),
                          activeServiceNeighborhoods_.end(), 0);
                break;
            }

            const double previousExcess = currentExcess;
            if (bestKind == MoveKind::MOVE) {
                const int oldService = current.getServiceForTask(bestTask1);
                if (scenario == ProbabilityScenario::Ps) {
                    prepareDistributionWithoutService(bestTask1, oldService);
                    const std::vector<double>& without =
                        distributionWithoutService[oldService];
                    const double newProbability =
                        matrix.getServiceProb(bestService);
                    for (int violations = 0; violations <= Vmax; ++violations) {
                        distribution[violations] =
                            without[violations] * (1.0 - newProbability)
                            + (violations > 0
                                ? without[violations - 1] * newProbability
                                : 0.0);
                    }
                    currentViolation = std::clamp(
                        currentViolation
                        + (newProbability - matrix.getServiceProb(oldService))
                            * without[Vmax],
                        0.0, 1.0);
                    std::fill(cachedWithoutService.begin(),
                              cachedWithoutService.end(), 0);
                }
                current.replaceService(
                    Task(bestTask1, matrix.getTaskConsumption(bestTask1)),
                    Service(bestService), matrix);
                activeServiceNeighborhoods_[oldService] = 1;
                activeServiceNeighborhoods_[bestService] = 1;
                currentExcess = bestNewExcess;
                if (scenario == ProbabilityScenario::Ps
                        && ++movesSinceProbabilityRebuild >= 32) {
                    distribution = probabilityDistribution(
                        current, matrix, Vmax);
                    currentViolation = std::clamp(
                        1.0 - std::accumulate(
                            distribution.begin(), distribution.end(), 0.0),
                        0.0, 1.0);
                    currentExcess = std::max(
                        0.0, currentViolation - Pmax);
                    movesSinceProbabilityRebuild = 0;
                    std::fill(cachedWithoutService.begin(),
                              cachedWithoutService.end(), 0);
                }
            } else {
                const int service1 = current.getServiceForTask(bestTask1);
                const int service2 = current.getServiceForTask(bestTask2);
                current.replaceService(
                    Task(bestTask1, matrix.getTaskConsumption(bestTask1)),
                    Service(service2), matrix);
                current.replaceService(
                    Task(bestTask2, matrix.getTaskConsumption(bestTask2)),
                    Service(service1), matrix);
                activeServiceNeighborhoods_[service1] = 1;
                activeServiceNeighborhoods_[service2] = 1;
            }

            if (currentExcess > 1e-12) {
                ++stats_.acceptedInfeasibleMoves;
                stats_.maximumAcceptedExcess = std::max(
                    stats_.maximumAcceptedExcess, currentExcess);
            } else if (previousExcess > 1e-12) {
                ++stats_.feasibleRecoveries;
            }
            appliedAnyMove = true;
            updateBestFeasible(current, currentExcess, best);
        }
        return appliedAnyMove;
    }

    void penalizeMaximumUtilityFeatures(const Allocation& localMinimum,
                                        const InstanceMatrix& matrix) {
        double maximumUtility = -std::numeric_limits<double>::infinity();
        std::vector<int> selectedTasks;
        for (int taskId = 0; taskId < matrix.getNumberOfTasks(); ++taskId) {
            const int serviceId = localMinimum.getServiceForTask(taskId);
            const double utility = matrix.getTaskCost(taskId, serviceId)
                / static_cast<double>(1 + penalties_[taskId][serviceId]);
            if (utility > maximumUtility + 1e-12) {
                maximumUtility = utility;
                selectedTasks.assign(1, taskId);
            } else if (std::abs(utility - maximumUtility) <= 1e-12) {
                selectedTasks.push_back(taskId);
            }
        }
        for (int taskId : selectedTasks) {
            const int serviceId = localMinimum.getServiceForTask(taskId);
            ++penalties_[taskId][serviceId];
            activeServiceNeighborhoods_[serviceId] = 1;
        }
    }

public:
    const SoftSlaSearchStats& stats() const { return stats_; }

    Allocation improve(Allocation initial, const InstanceMatrix& matrix,
                       int Vmax, int Smax, double Pmax,
                       ProbabilityScenario scenario,
                       double glsAlpha = 0.3,
                       double probabilityBeta = 0.05,
                       int penaltyRounds = 30,
                       double deadlineMs = std::numeric_limits<double>::infinity(),
                       bool allowSoftSla = true,
                       double softSlaStartMs = 0.0,
                       double proximityFraction = 1.0) {
        const int numberOfTasks = matrix.getNumberOfTasks();
        const int numberOfServices = matrix.getNumberOfServices();
        penalties_.assign(numberOfTasks, std::vector<int>(numberOfServices, 0));
        activeServiceNeighborhoods_.assign(numberOfServices, 1);
        lambda_ = glsAlpha * std::max(1.0, initial.getCurrentCost())
            / std::max(1, numberOfTasks);

        // Um excesso igual a Pmax recebe inicialmente uma penalidade equivalente
        // a beta vezes o custo inicial. mu oscila para permitir a entrada na região
        // inviável e depois aumentar a pressão de retorno à viabilidade.
        probabilityPenalty_ = probabilityBeta
            * std::max(1.0, initial.getCurrentCost())
            / std::max(Pmax, 1e-9);
        minimumProbabilityPenalty_ = probabilityPenalty_ * 0.1;
        stats_ = {};

        Allocation current = initial;
        Allocation best = initial;
        std::vector<double> distribution;
        double currentViolation = 0.0;
        double currentExcess = 0.0;
        int movesSinceProbabilityRebuild = 0;
        if (scenario == ProbabilityScenario::Ps) {
            distribution = probabilityDistribution(current, matrix, Vmax);
            currentViolation = std::clamp(
                1.0 - std::accumulate(
                    distribution.begin(), distribution.end(), 0.0),
                0.0, 1.0);
            currentExcess = std::max(0.0, currentViolation - Pmax);
        } else {
            currentExcess = violationExcess(
                current, matrix, Vmax, Pmax, scenario);
        }

        for (int round = 0; round < penaltyRounds; ++round) {
            if (deadlineReached(deadlineMs))
                break;

            bool softSlaEnabled = false;
            if (allowSoftSla && nowMs() >= softSlaStartMs) {
                const double allowedSlack =
                    std::max(0.0, proximityFraction) * Pmax;
                softSlaEnabled = currentViolation + allowedSlack
                    >= Pmax - 1e-12;
            }
            guidedDescent(current, best, currentExcess,
                          currentViolation, distribution,
                          movesSinceProbabilityRebuild, matrix,
                          Vmax, Smax, Pmax, scenario,
                          softSlaEnabled, deadlineMs);
            if (deadlineReached(deadlineMs))
                break;

            penalizeMaximumUtilityFeatures(current, matrix);
            ++stats_.completedPenaltyRounds;
            if (softSlaEnabled) {
                if (currentExcess > 1e-12)
                    probabilityPenalty_ *= 2.0;
                else
                    probabilityPenalty_ = std::max(
                        minimumProbabilityPenalty_, probabilityPenalty_ * 0.8);
            }
        }
        if (scenario == ProbabilityScenario::Ps) {
            const std::vector<double> exactDistribution =
                probabilityDistribution(current, matrix, Vmax);
            const double exactViolation = std::clamp(
                1.0 - std::accumulate(
                    exactDistribution.begin(), exactDistribution.end(), 0.0),
                0.0, 1.0);
            stats_.finalProbabilityDrift =
                std::abs(exactViolation - currentViolation);
        }
        return best;
    }
};
