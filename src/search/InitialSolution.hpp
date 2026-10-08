#pragma once

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <vector>

#include "Allocation.h"
#include "InstanceMatrix.hpp"
#include "ProbabilityScenario.h"
#include "Service.h"
#include "SolutionValidator.hpp"
#include "Task.h"

// Construção inicial determinística: cada tarefa vai para o serviço de menor
// probabilidade de violação de SLA que mantém a solução viável. Prioriza a
// viabilidade, não o custo; a GLS parte daqui para reduzir o custo.
inline Allocation buildInitialSolution(const InstanceMatrix& matrix) {
    SolutionValidator validator;
    ProbabilityScenario scenario = ProbabilityScenario::Ps;
    Allocation allocation;
    std::vector<int> services(matrix.getNumberOfServices());
    std::iota(services.begin(), services.end(), 0);
    std::stable_sort(services.begin(), services.end(),
        [&](int first, int second) {
            return matrix.getServiceProb(first) < matrix.getServiceProb(second);
        });

    for (int taskId = 0; taskId < matrix.getNumberOfTasks(); ++taskId) {
        Task task(taskId, matrix.getTaskConsumption(taskId));
        bool assigned = false;
        for (int serviceId : services) {
            allocation.addTask(task, Service(serviceId), matrix);
            if (validator.isFeasible(
                    matrix, allocation, matrix.getVmax(), matrix.getSmax(),
                    matrix.getPmax(), scenario, true)) {
                assigned = true;
                break;
            }
            allocation.removeTask(task, matrix);
        }
        if (!assigned)
            throw std::runtime_error("Não foi possível construir solução viável para "
                                     + matrix.getInstanceName());
    }
    return allocation;
}

inline bool isFeasible(const Allocation& solution, const InstanceMatrix& matrix) {
    SolutionValidator validator;
    ProbabilityScenario scenario = ProbabilityScenario::Ps;
    Allocation copy = solution;
    return validator.isFeasible(
        matrix, copy, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(),
        scenario, true);
}
