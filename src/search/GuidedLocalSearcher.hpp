#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <vector>

#include "Allocation.h"
#include "InstanceMatrix.hpp"
#include "ProbabilityScenario.h"
#include "Service.h"
#include "SolutionValidator.hpp"
#include "Task.h"

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

            enum class MoveKind { NONE, MOVE, SWAP };
            MoveKind bestKind = MoveKind::NONE;
            double bestDelta = -1e-9;
            int bestTask1 = -1;
            int bestTask2 = -1;
            int bestService = -1;
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
            } else {
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

        for (int taskId = 0; taskId < matrix.getNumberOfTasks(); ++taskId) {
            const int serviceId = localMinimum.getServiceForTask(taskId);
            if (serviceId < 0)
                continue;
            const double utility = matrix.getTaskCost(taskId, serviceId)
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
            guidedDescent(current, best, matrix, Vmax, Smax, Pmax, pScenario,
                          deadlineMs);
            if (deadlineReached(deadlineMs))
                break;
            penalizeMaximumUtilityFeatures(current, matrix);
        }
        return best;
    }
};
