#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <functional>
#include <numeric>
#include <vector>

#include "Allocation.h"
#include "InstanceMatrix.hpp"
#include "ProbabilityScenario.h"
#include "Service.h"
#include "SolutionValidator.hpp"
#include "Task.h"

#ifndef GLS_CAPACITY_WEIGHT
#define GLS_CAPACITY_WEIGHT 0.0
#endif

#ifndef GLS_REGRET_WEIGHT
#define GLS_REGRET_WEIGHT 0.0
#endif

#ifndef GLS_RECONSTRUCTION_SIZE
#define GLS_RECONSTRUCTION_SIZE 3
#endif

#ifndef GLS_RECONSTRUCTION_PERIOD
#define GLS_RECONSTRUCTION_PERIOD 5
#endif

// Guided Local Search for task->service assignment features. Penalties guide
// the trajectory, while the returned allocation is always selected by real cost.
class GuidedLocalSearcher {
    SolutionValidator validator_;
    std::vector<std::vector<int>> penalties_;
#ifdef ENABLE_GUIDED_FAST_LOCAL_SEARCH
    // The sub-neighborhood of a service contains every MOVE/SWAP that enters
    // or leaves it. Penalized features reactivate their service neighborhood.
    std::vector<unsigned char> activeServiceNeighborhoods_;
#endif
    double lambda_ = 0.0;

    static double nowMs() {
        using namespace std::chrono;
        return static_cast<double>(duration_cast<milliseconds>(
            steady_clock::now().time_since_epoch()).count());
    }

    static bool deadlineReached(double deadlineMs) {
        return std::isfinite(deadlineMs) && nowMs() >= deadlineMs;
    }

    void updateBest(const Allocation& candidate, Allocation& best) const {
        if (candidate.getCurrentCost() < best.getCurrentCost() - 1e-9)
            best = candidate;
    }

    bool guidedDescent(Allocation& current, Allocation& best,
                       const InstanceMatrix& matrix,
                       int Vmax, int Smax, double Pmax,
                       ProbabilityScenario pScenario, double deadlineMs) {
        const int numberOfTasks = matrix.getNumberOfTasks();
        const int numberOfServices = matrix.getNumberOfServices();
        bool appliedAnyMove = false;

#ifdef ENABLE_GUIDED_FAST_LOCAL_SEARCH
        if (static_cast<int>(activeServiceNeighborhoods_.size()) != numberOfServices)
            activeServiceNeighborhoods_.assign(numberOfServices, 1);
        if (std::none_of(activeServiceNeighborhoods_.begin(),
                         activeServiceNeighborhoods_.end(),
                         [](unsigned char active) { return active != 0; }))
            return false;
#endif

        while (true) {
            if (deadlineReached(deadlineMs))
                break;

            enum class MoveKind { NONE, MOVE, SWAP, EJECTION_CHAIN };
            MoveKind bestKind = MoveKind::NONE;
            double bestDelta = -1e-9;
            int bestTask1 = -1;
            int bestTask2 = -1;
            int bestService = -1;
            int bestService2 = -1;
            bool timedOut = false;

            for (int taskId = 0; taskId < numberOfTasks; ++taskId) {
                if (deadlineReached(deadlineMs)) {
                    timedOut = true;
                    break;
                }
                const int oldService = current.getServiceForTask(taskId);
                if (oldService < 0)
                    continue;

                for (int newService = 0; newService < numberOfServices; ++newService) {
                    if (newService == oldService)
                        continue;
#ifdef ENABLE_GUIDED_FAST_LOCAL_SEARCH
                    if (!activeServiceNeighborhoods_[oldService]
                            && !activeServiceNeighborhoods_[newService])
                        continue;
#endif

                    const double deltaCost =
                        matrix.getTaskCost(taskId, newService)
                        - matrix.getTaskCost(taskId, oldService);
                    const double deltaPenalty =
                        penalties_[taskId][newService]
                        - penalties_[taskId][oldService];
                    const double deltaGuided = deltaCost + lambda_ * deltaPenalty;
                    if (deltaGuided >= bestDelta)
                        continue;

                    Task task(taskId, matrix.getTaskConsumption(taskId));
                    current.replaceService(task, Service(newService), matrix);
                    const bool feasible = validator_.isFeasible(
                        matrix, current, Vmax, Smax, Pmax, pScenario, true);
                    current.replaceService(task, Service(oldService), matrix);

                    if (feasible && deltaGuided < bestDelta) {
                        bestDelta = deltaGuided;
                        bestKind = MoveKind::MOVE;
                        bestTask1 = taskId;
                        bestTask2 = -1;
                        bestService = newService;
                    }
                }
            }

            if (timedOut)
                break;

            for (int task1 = 0; task1 < numberOfTasks; ++task1) {
                if (deadlineReached(deadlineMs)) {
                    timedOut = true;
                    break;
                }
                const int service1 = current.getServiceForTask(task1);
                if (service1 < 0)
                    continue;

                for (int task2 = task1 + 1; task2 < numberOfTasks; ++task2) {
                    const int service2 = current.getServiceForTask(task2);
                    if (service2 < 0 || service1 == service2)
                        continue;
#ifdef ENABLE_GUIDED_FAST_LOCAL_SEARCH
                    if (!activeServiceNeighborhoods_[service1]
                            && !activeServiceNeighborhoods_[service2])
                        continue;
#endif

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

                    Task first(task1, matrix.getTaskConsumption(task1));
                    Task second(task2, matrix.getTaskConsumption(task2));
                    current.replaceService(first, Service(service2), matrix);
                    current.replaceService(second, Service(service1), matrix);
                    const bool feasible = validator_.isFeasible(
                        matrix, current, Vmax, Smax, Pmax, pScenario, true);
                    current.replaceService(first, Service(service1), matrix);
                    current.replaceService(second, Service(service2), matrix);

                    if (feasible && deltaGuided < bestDelta) {
                        bestDelta = deltaGuided;
                        bestKind = MoveKind::SWAP;
                        bestTask1 = task1;
                        bestTask2 = task2;
                        bestService = -1;
                    }
                }
            }

            if (timedOut)
                break;

#ifdef ENABLE_GLS_EJECTION_CHAIN
            // Cadeia curta A->B, B->C. Diferentemente de SWAP, libera espaco
            // no destino de task1 deslocando task2 para um terceiro servico.
            // Ela e uma vizinhanca de escape: so e examinada no minimo local
            // de MOVE/SWAP. A capacidade e prefiltrada; Smax e SLA sao exatos.
            if (bestKind == MoveKind::NONE) {
              const std::vector<int>& resourceUse = current.getResourcePerService();
              const int resourceCapacity = matrix.getVres();
              for (int task1 = 0; task1 < numberOfTasks; ++task1) {
                if (deadlineReached(deadlineMs)) {
                    timedOut = true;
                    break;
                }
                const int service1 = current.getServiceForTask(task1);
                if (service1 < 0)
                    continue;
                const int consumption1 = matrix.getTaskConsumption(task1);

                for (int middleService = 0; middleService < numberOfServices;
                     ++middleService) {
                    if (middleService == service1)
                        continue;
                    // Se task1 cabe diretamente, MOVE ja cobre a transicao.
                    // A cadeia e reservada ao caso em que expulsar task2 e
                    // realmente necessario para vencer a barreira de capacidade.
                    if (resourceUse[middleService] + consumption1
                            <= resourceCapacity)
                        continue;
#ifdef ENABLE_GUIDED_FAST_LOCAL_SEARCH
                    if (!activeServiceNeighborhoods_[service1]
                            && !activeServiceNeighborhoods_[middleService])
                        continue;
#endif
                    for (int task2 = 0; task2 < numberOfTasks; ++task2) {
                        if (task2 == task1
                                || current.getServiceForTask(task2) != middleService)
                            continue;
                        const int consumption2 = matrix.getTaskConsumption(task2);
                        if (resourceUse[middleService] + consumption1
                                - consumption2 > resourceCapacity)
                            continue;

                        for (int service3 = 0; service3 < numberOfServices;
                             ++service3) {
                            if (service3 == service1
                                    || service3 == middleService)
                                continue;
                            if (resourceUse[service3] + consumption2
                                    > resourceCapacity)
                                continue;

                            const double deltaCost =
                                matrix.getTaskCost(task1, middleService)
                                + matrix.getTaskCost(task2, service3)
                                - matrix.getTaskCost(task1, service1)
                                - matrix.getTaskCost(task2, middleService);
                            const double deltaPenalty =
                                penalties_[task1][middleService]
                                + penalties_[task2][service3]
                                - penalties_[task1][service1]
                                - penalties_[task2][middleService];
                            const double deltaGuided = deltaCost
                                + lambda_ * deltaPenalty;
                            if (deltaGuided >= bestDelta)
                                continue;

                            Task first(task1, consumption1);
                            Task second(task2, consumption2);
                            current.replaceService(second, Service(service3), matrix);
                            current.replaceService(first, Service(middleService), matrix);
                            const bool feasible = validator_.isFeasible(
                                matrix, current, Vmax, Smax, Pmax,
                                pScenario, true);
                            current.replaceService(first, Service(service1), matrix);
                            current.replaceService(second, Service(middleService), matrix);

                            if (feasible && deltaGuided < bestDelta) {
                                bestDelta = deltaGuided;
                                bestKind = MoveKind::EJECTION_CHAIN;
                                bestTask1 = task1;
                                bestTask2 = task2;
                                bestService = middleService;
                                bestService2 = service3;
                            }
                        }
                    }
                }
              }
            }
#endif

            if (timedOut)
                break;

            if (bestKind == MoveKind::NONE) {
#ifdef ENABLE_GUIDED_FAST_LOCAL_SEARCH
                std::fill(activeServiceNeighborhoods_.begin(),
                          activeServiceNeighborhoods_.end(), 0);
#endif
                break;
            }

            if (bestKind == MoveKind::MOVE) {
#ifdef ENABLE_GUIDED_FAST_LOCAL_SEARCH
                const int oldService = current.getServiceForTask(bestTask1);
#endif
                current.replaceService(
                    Task(bestTask1, matrix.getTaskConsumption(bestTask1)),
                    Service(bestService), matrix);
#ifdef ENABLE_GUIDED_FAST_LOCAL_SEARCH
                activeServiceNeighborhoods_[oldService] = 1;
                activeServiceNeighborhoods_[bestService] = 1;
#endif
            } else if (bestKind == MoveKind::SWAP) {
                const int service1 = current.getServiceForTask(bestTask1);
                const int service2 = current.getServiceForTask(bestTask2);
                current.replaceService(
                    Task(bestTask1, matrix.getTaskConsumption(bestTask1)),
                    Service(service2), matrix);
                current.replaceService(
                    Task(bestTask2, matrix.getTaskConsumption(bestTask2)),
                    Service(service1), matrix);
#ifdef ENABLE_GUIDED_FAST_LOCAL_SEARCH
                activeServiceNeighborhoods_[service1] = 1;
                activeServiceNeighborhoods_[service2] = 1;
#endif
            } else {
                const int oldService = current.getServiceForTask(bestTask1);
                current.replaceService(
                    Task(bestTask2, matrix.getTaskConsumption(bestTask2)),
                    Service(bestService2), matrix);
                current.replaceService(
                    Task(bestTask1, matrix.getTaskConsumption(bestTask1)),
                    Service(bestService), matrix);
#ifdef ENABLE_GUIDED_FAST_LOCAL_SEARCH
                activeServiceNeighborhoods_[oldService] = 1;
                activeServiceNeighborhoods_[bestService] = 1;
                activeServiceNeighborhoods_[bestService2] = 1;
#endif
            }

            appliedAnyMove = true;
            updateBest(current, best);
        }

        return appliedAnyMove;
    }

    void penalizeMaximumUtilityFeatures(const Allocation& localMinimum,
                                        const InstanceMatrix& matrix) {
        double maximumUtility = -std::numeric_limits<double>::infinity();
        std::vector<int> selectedTasks;
        double meanMinimumCost = 0.0;
        for (int taskId = 0; taskId < matrix.getNumberOfTasks(); ++taskId)
            meanMinimumCost += matrix.getMinCostForTask(taskId);
        meanMinimumCost /= std::max(1, matrix.getNumberOfTasks());

        const int resourceCapacity = std::max(1, matrix.getVres());
        const std::vector<int>& resourceUse = localMinimum.getResourcePerService();

        for (int taskId = 0; taskId < matrix.getNumberOfTasks(); ++taskId) {
            const int serviceId = localMinimum.getServiceForTask(taskId);
            if (serviceId < 0)
                continue;
            const double assignmentCost = matrix.getTaskCost(taskId, serviceId);
            const double pressure = std::clamp(
                resourceUse[serviceId] / static_cast<double>(resourceCapacity),
                0.0, 1.0);
            const double taskWeight = matrix.getTaskConsumption(taskId)
                / static_cast<double>(resourceCapacity);
            const double regret = std::max(
                0.0, assignmentCost - matrix.getMinCostForTask(taskId));

            // A utilidade original usa somente assignmentCost. Os termos
            // opcionais priorizam features ligadas ao gargalo real: tarefas
            // pesadas em serviços saturados e atribuições com alto arrependimento.
            const double structuralCost = assignmentCost
                + GLS_CAPACITY_WEIGHT * meanMinimumCost * pressure
                    * (1.0 + taskWeight)
                + GLS_REGRET_WEIGHT * regret;
            const double utility = structuralCost
                / static_cast<double>(1 + penalties_[taskId][serviceId]);

            if (utility > maximumUtility + 1e-12) {
                maximumUtility = utility;
                selectedTasks.clear();
                selectedTasks.push_back(taskId);
            } else if (std::abs(utility - maximumUtility) <= 1e-12) {
                selectedTasks.push_back(taskId);
            }
        }

        for (int taskId : selectedTasks) {
            const int serviceId = localMinimum.getServiceForTask(taskId);
            ++penalties_[taskId][serviceId];
#ifdef ENABLE_GUIDED_FAST_LOCAL_SEARCH
            activeServiceNeighborhoods_[serviceId] = 1;
#endif
        }
    }

#ifdef ENABLE_GLS_PARTIAL_RECONSTRUCTION
    // Destroy-and-repair exato em um grupo pequeno. A selecao privilegia
    // features penalizadas e caras; a reinsercao conjunta permite atravessar
    // barreiras que MOVE, SWAP e uma cadeia curta nao representam.
    bool partialReconstruction(Allocation& current, Allocation& best,
                               const InstanceMatrix& matrix,
                               int Vmax, int Smax, double Pmax,
                               ProbabilityScenario pScenario,
                               double deadlineMs) {
        const int numberOfTasks = matrix.getNumberOfTasks();
        const int numberOfServices = matrix.getNumberOfServices();
        const int destroySize = std::min(GLS_RECONSTRUCTION_SIZE, numberOfTasks);
        if (destroySize <= 0 || deadlineReached(deadlineMs))
            return false;

        std::vector<int> ranked(numberOfTasks);
        std::iota(ranked.begin(), ranked.end(), 0);
        std::sort(ranked.begin(), ranked.end(), [&](int lhs, int rhs) {
            const int lhsService = current.getServiceForTask(lhs);
            const int rhsService = current.getServiceForTask(rhs);
            const double lhsScore = lambda_ * penalties_[lhs][lhsService]
                + matrix.getTaskCost(lhs, lhsService);
            const double rhsScore = lambda_ * penalties_[rhs][rhsService]
                + matrix.getTaskCost(rhs, rhsService);
            return lhsScore > rhsScore;
        });
        ranked.resize(destroySize);

        double originalRealContribution = 0.0;
        for (int taskId : ranked) {
            const int serviceId = current.getServiceForTask(taskId);
            originalRealContribution += matrix.getTaskCost(taskId, serviceId);
        }

        Allocation partial = current;
        for (int taskId : ranked)
            partial.removeTask(Task(taskId, matrix.getTaskConsumption(taskId)), matrix);

        Allocation bestCandidate;
        bool found = false;
        // As penalidades escolhem o fragmento destruido, mas o repair otimiza
        // custo real. Isso impede que a LNS apenas replique a diversificacao
        // do GLS e degrade a incumbente em troca de menor custo penalizado.
        double bestContribution = originalRealContribution - 1e-9;
        std::function<void(int, Allocation&, double)> repair =
            [&](int position, Allocation& candidate, double contribution) {
                if (deadlineReached(deadlineMs) || contribution >= bestContribution)
                    return;
                if (position == destroySize) {
                    if (validator_.isFeasible(matrix, candidate, Vmax, Smax,
                                              Pmax, pScenario, true)) {
                        bestContribution = contribution;
                        bestCandidate = candidate;
                        found = true;
                    }
                    return;
                }

                const int taskId = ranked[position];
                const int consumption = matrix.getTaskConsumption(taskId);
                std::vector<int> services(numberOfServices);
                std::iota(services.begin(), services.end(), 0);
                std::sort(services.begin(), services.end(), [&](int lhs, int rhs) {
                    return matrix.getTaskCost(taskId, lhs)
                        < matrix.getTaskCost(taskId, rhs);
                });
                for (int serviceId : services) {
                    if (candidate.getResourcePerService()[serviceId] + consumption
                            > matrix.getVres())
                        continue;
                    const double featureContribution =
                        matrix.getTaskCost(taskId, serviceId);
                    candidate.addTask(Task(taskId, consumption),
                                      Service(serviceId), matrix);
                    repair(position + 1, candidate,
                           contribution + featureContribution);
                    candidate.removeTask(Task(taskId, consumption), matrix);
                    if (deadlineReached(deadlineMs))
                        return;
                }
            };
        repair(0, partial, 0.0);

        if (!found)
            return false;
        current = bestCandidate;
#ifdef ENABLE_GUIDED_FAST_LOCAL_SEARCH
        activeServiceNeighborhoods_.assign(numberOfServices, 1);
#endif
        updateBest(current, best);
        return true;
    }
#endif

public:
    Allocation improve(Allocation initial, const InstanceMatrix& matrix,
                       int Vmax, int Smax, double Pmax,
                       ProbabilityScenario pScenario,
                       double alpha = 0.3, int penaltyRounds = 30,
                       bool preservePenalties = false,
                       double deadlineMs = std::numeric_limits<double>::infinity()) {
        const int numberOfTasks = matrix.getNumberOfTasks();
        const int numberOfServices = matrix.getNumberOfServices();
        const bool dimensionsChanged =
            static_cast<int>(penalties_.size()) != numberOfTasks
            || (!penalties_.empty()
                && static_cast<int>(penalties_.front().size()) != numberOfServices);

        if (!preservePenalties || dimensionsChanged || penalties_.empty()) {
            penalties_.assign(numberOfTasks, std::vector<int>(numberOfServices, 0));
            lambda_ = alpha * std::max(1.0, initial.getCurrentCost())
                / std::max(1, numberOfTasks);
        }
#ifdef ENABLE_GUIDED_FAST_LOCAL_SEARCH
        // Normal search, oscillation and perturbation may change the allocation
        // between calls, so activation bits do not persist with the penalties.
        activeServiceNeighborhoods_.assign(numberOfServices, 1);
#endif

        Allocation current = initial;
        Allocation best = initial;
        for (int round = 0; round < penaltyRounds; ++round) {
            if (deadlineReached(deadlineMs))
                break;
            guidedDescent(current, best, matrix, Vmax, Smax, Pmax,
                          pScenario, deadlineMs);
            if (deadlineReached(deadlineMs))
                break;
            penalizeMaximumUtilityFeatures(current, matrix);
#ifdef ENABLE_GLS_PARTIAL_RECONSTRUCTION
            if ((round + 1) % GLS_RECONSTRUCTION_PERIOD == 0
                    && !deadlineReached(deadlineMs))
                partialReconstruction(current, best, matrix, Vmax, Smax,
                                      Pmax, pScenario, deadlineMs);
#endif
        }
        return best;
    }
};
