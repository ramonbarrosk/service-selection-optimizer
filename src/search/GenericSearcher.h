
#pragma once
#include <map>
#include <vector>
#include <climits>
#include "Allocation.h"
#include "InstanceMatrix.hpp"
#include "SolutionValidator.hpp"
#include "ImprovementCondition.h"
#include "ImprovementMode.h"
#include "ProbabilityScenario.h"
#include "RandomUtil.hpp"

using std::vector;


class GenericSearcher {
    SolutionValidator validator;
public:
    GenericSearcher() = default;

    bool costImprovement(Allocation& all, const InstanceMatrix& matrix, int Vmax, int Smax, double Pmax, ProbabilityScenario pScenario, ImprovementCondition condition, ImprovementMode mode) {
       bool globallyImproved = false;


       // MODO MOVE - tenta mover cada tarefa para um serviço diferente, buscando redução de custo, e aceita o primeiro movimento que melhorar a solução (First Improvement) ou o melhor movimento encontrado (Best Improvement)

       if (mode == ImprovementMode::MOVE) {
            bool locallyImproved;

            do {
                locallyImproved = false;

                int bestTaskIdMove = -1;
                int bestServiceIdMove = -1;
                int bestCostGain = -1;
                bool bestFound = false;

                for (auto& [taskId, currentServId] : all.getAllocation()) {
                   int init = RandomUtil::getRandomInt(0, matrix.getNumberOfServices() - 1);

                   auto tryMove = [&](int i) -> bool {
                        if (i == currentServId)
                            return false;

                        if (matrix.getTaskCost(taskId, i) >= matrix.getTaskCost(taskId, currentServId))
                            return false;

                        Task task(taskId, matrix.getTaskConsumption(taskId));
                        Service newService(i);
                        Service currentService(currentServId);

                        all.replaceService(task, newService, matrix);

                        bool probRestrictionRequired = matrix.getServiceProb(i) > matrix.getServiceProb(currentServId);

                        if (validator.isFeasible(matrix, all, Vmax, Smax, Pmax, pScenario, probRestrictionRequired)) {
                            if (condition == ImprovementCondition::FIRST_IMPROVEMENT) { // Verificar o uso desses ENUM
                                // Aceita imediatamente o primeiro movimento válido
                                locallyImproved = true;
                                globallyImproved = true;
                                return true;
                            } else {
                                // Best Improvement: desfaz e guarda se for o melhor até agora
                                all.replaceService(task, currentService, matrix);

                                int costGain = matrix.getTaskCost(taskId, currentServId) - matrix.getTaskCost(taskId, i);

                                if (costGain > bestCostGain) {
                                    bestCostGain = costGain;
                                    bestTaskIdMove = taskId;
                                    bestServiceIdMove = i;
                                    bestFound = true;
                                }
                            }
                        } else {
                            // Movimento inviável — desfaz
                            all.replaceService(task, currentService, matrix);
                        }

                        return false;
                   };

                   bool stopped = false;

                   for (int i = init; i < matrix.getNumberOfServices() && !stopped; ++i)
                        stopped = tryMove(i);

                    if (!stopped &&
                        (condition == ImprovementCondition::BEST_IMPROVEMENT ||
                        (condition == ImprovementCondition::FIRST_IMPROVEMENT && !locallyImproved))) {

                        for (int i = 0; i < init && !stopped; ++i)
                            stopped = tryMove(i);

                    }

                    if (locallyImproved) break; // FirstImprovement: sai do loop de tarefas
                }

                if (bestFound && condition == ImprovementCondition::BEST_IMPROVEMENT) {
                    // Aplica o melhor movimento encontrado
                    locallyImproved = true;
                    globallyImproved = true;
                    all.replaceService(Task(bestTaskIdMove, matrix.getTaskConsumption(bestTaskIdMove)), Service(bestServiceIdMove), matrix);
                }
            } while (locallyImproved && condition == ImprovementCondition::BEST_IMPROVEMENT);

            return globallyImproved;
       } else {

            // MODO SWAP - tenta trocar o serviço de cada tarefa com o serviço de outra tarefa, buscando redução de custo, e aceita a primeira troca que melhorar a solução (First Improvement) ou a melhor troca encontrada (Best Improvement)

            bool locallyImproved;

            do {
                locallyImproved = false;

                int t1Id = -1, t2Id = -1, s1Id = -1, s2Id = -1;
                int bestCostGain = -1;
                bool bestFound = false;

                for (auto& [currentTaskId, currentTaskServId] : all.getAllocation()) {
                    int init = RandomUtil::getRandomInt(0, matrix.getNumberOfTasks() - 1);

                    auto trySwap = [&](int i) -> bool {
                        if (i == currentTaskId) return false;

                        int randomTaskServId = all.getServiceForTask(i);

                        if (currentTaskServId == randomTaskServId)
                            return false;

                        int costBefore = matrix.getTaskCost(currentTaskId, currentTaskServId) +
                                         matrix.getTaskCost(i, randomTaskServId);

                        int costAfter = matrix.getTaskCost(currentTaskId, randomTaskServId) +
                                        matrix.getTaskCost(i, currentTaskServId);

                        if (costAfter >= costBefore)
                            return false;

                        Task currentTask(currentTaskId, matrix.getTaskConsumption(currentTaskId));
                        Task randomTask(i, matrix.getTaskConsumption(i));
                        Service currentService(currentTaskServId);
                        Service randomTaskService(randomTaskServId);

                        // Executa o SWAP
                        all.replaceService(currentTask, randomTaskService, matrix);
                        all.replaceService(randomTask, currentService, matrix);

                        if (validator.isFeasible(matrix, all, Vmax, Smax, Pmax, pScenario, false)) {
                            if (condition == ImprovementCondition::FIRST_IMPROVEMENT) {
                                // Aceita imediatamente a primeira troca válida
                                locallyImproved = true;
                                globallyImproved = true;
                                return true;
                            } else {
                                // Best Improvement: desfaz e guarda se for a melhor até agora
                                all.replaceService(currentTask, currentService, matrix);
                                all.replaceService(randomTask, randomTaskService, matrix);

                                int costGain = costBefore - costAfter;

                                if (costGain > bestCostGain) {
                                    bestCostGain = costGain;
                                    t1Id = currentTaskId;
                                    s1Id = randomTaskServId;
                                    t2Id = i;
                                    s2Id = currentTaskServId;
                                    bestFound = true;
                                }
                            }
                        } else {
                            // Troca inviável — desfaz
                            all.replaceService(currentTask, currentService, matrix);
                            all.replaceService(randomTask, randomTaskService, matrix);
                        }

                        return false;
                    };

                    bool stopped = false;

                    for (int i = init; i < matrix.getNumberOfTasks() && !stopped; ++i)
                        stopped = trySwap(i);

                    if (!stopped &&
                        (condition == ImprovementCondition::BEST_IMPROVEMENT ||
                        (condition == ImprovementCondition::FIRST_IMPROVEMENT && !locallyImproved))) {
                        for (int i = 0; i < init && !stopped; ++i)
                            stopped = trySwap(i);
                    }

                    if (locallyImproved) break; // FirstImprovement: sai do loop de tarefas
                }


                // Best improvement: Aplica a melhor troca encontrada após avaliar todas as possibilidades

                if (bestFound && condition == ImprovementCondition::BEST_IMPROVEMENT) {
                    // Aplica a melhor troca encontrada
                    locallyImproved = true;
                    globallyImproved = true;
                    all.replaceService(Task(t1Id, matrix.getTaskConsumption(t1Id)), Service(s1Id), matrix);
                    all.replaceService(Task(t2Id, matrix.getTaskConsumption(t2Id)), Service(s2Id), matrix);
                }

            } while (locallyImproved && condition == ImprovementCondition::BEST_IMPROVEMENT);

            return globallyImproved;
       }
    }

    // VND - Variable Neighborhood Descent: aplica iterativamente as melhorias de MOVE e SWAP até não encontrar mais melhorias em ambos os modos


    // IMPORTANTE: PODEMOS TENTAR FAZER ALGUMA MUDANÇA AQUI???

    Allocation VND(Allocation initialAllocation, const InstanceMatrix& matrix, int Vmax, int Smax, double Pmax, ProbabilityScenario pScenario, ImprovementCondition condition) {
        Allocation currentAllocation = initialAllocation;

        int k = 1;
        bool improved = false;

        while (k <= 2) {
            if (k == 1) {

                // Vizinhança 1: MOVE - tenta mover cada tarefa para um serviço diferente, buscando redução de custo, e aceita o primeiro movimento
                improved = costImprovement(currentAllocation, matrix, Vmax, Smax, Pmax, pScenario, condition, ImprovementMode::MOVE);

                k = improved ? 1 : 2; // Se melhorou, continua no mesmo modo; senão, passa para o próximo modo
            } else {

                // Vizinhança 2: SWAP - tenta trocar o serviço de cada tarefa com o serviço de outra tarefa, buscando redução de custo, e aceita a primeira troca que melhorar a solução
                improved = costImprovement(currentAllocation, matrix, Vmax, Smax, Pmax, pScenario, condition, ImprovementMode::SWAP);

                k = improved ? 1 : 3; // Se melhorou, volta para o modo MOVE; senão, termina o processo
            }
        }

        return currentAllocation;
    }
};
