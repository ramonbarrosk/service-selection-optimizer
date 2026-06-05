#pragma once
#include <map>
#include <vector>
#include <algorithm>
#include <chrono>
#include <climits>
#include "Allocation.h"
#include "InstanceMatrix.hpp"
#include "SolutionValidator.hpp"
#include "ImprovementCondition.h"
#include "ImprovementHeuristic.h"
#include "ImprovementMode.h"
#include "ProbabilityScenario.h"
#include "RandomUtil.hpp"
#include "SearchMode.h"
#include "GenericSearcher.h"
#include "PertubationMode.h"

class ILS {

    SolutionValidator validator;

    static double nowMs() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }

public:
    ILS() = default;


    // ILS com restart se a solução não melhorar por um número determinado de iterações

    Allocation ILSWithRestart(const InstanceMatrix& matrix, double alpha, int IT_MAX, double instanceInitTime, ProbabilityScenario pScenario, ImprovementHeuristic ImprovementHeuristic, SearchMode searchMode, ImprovementCondition improvementCondition, ImprovementMode improvementMode, PerturbationMode pertubationMode) {
        Allocation currentAllocation = greedyInitialSolution(matrix, alpha, matrix.getNumberOfTasks(), matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario);

        currentAllocation = neighborhoodSearch(matrix, currentAllocation, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario, searchMode, improvementCondition, ImprovementHeuristic, improvementMode);

        Allocation bestAllocation = currentAllocation;
        double bestCost = currentAllocation.getCurrentCost();
        bestAllocation.setTimeToBest((nowMs() - instanceInitTime) / 1000.0);

        int contNotImproved = 0;
        int restartStreak   = 0;  // reinicios consecutivos sem melhora global

        for (int i = 0; i < IT_MAX; ++i) {
            currentAllocation = pertubation(currentAllocation, matrix, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario, i, IT_MAX, pertubationMode);

            currentAllocation = neighborhoodSearch(matrix, currentAllocation, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario, searchMode, improvementCondition, ImprovementHeuristic, improvementMode);

            if (currentAllocation.getCurrentCost() < bestCost) {
                bestAllocation = currentAllocation;
                bestCost = currentAllocation.getCurrentCost();
                bestAllocation.setTimeToBest((nowMs() - instanceInitTime) / 1000.0);
                contNotImproved = 0;
                restartStreak   = 0;  // melhora global: volta ao construtivo por custo
            } else {
                contNotImproved++;
            }

            if (contNotImproved > IT_MAX / 10) {
                contNotImproved = 0;

                // IGrAl (Caramia 2008): mergulha em soluções inviáveis para escapar do ótimo local.
                // Em vez de reparar deterministicamente (o que desfaz a diversidade), penaliza a
                // solução inviável e deixa o VND encontrar o caminho de volta à viabilidade por conta
                // própria — explorando uma bacia diferente da já varrida.
                Allocation diveAllocation = pertubationInfeasible(currentAllocation, matrix, matrix.getSmax(), i, IT_MAX);

                double violationExcess = validator.computeViolationExcess(
                    matrix, diveAllocation, matrix.getVmax(), matrix.getPmax(), pScenario);
                // λ escala o excesso de violação na mesma ordem de grandeza dos custos:
                // excesso igual a Pmax ≅ penalidade igual a bestCost.
                double lambda = bestCost / std::max(matrix.getPmax(), 1e-9);
                double penalizedCost = diveAllocation.getCurrentCost() + lambda * violationExcess;

                // w cresce com reinicios consecutivos sem melhora: começa guiado por custo
                // (w=0) e migra progressivamente para guiado por probabilidade (w=1) a cada
                // reinicio frustrado, diversificando a região de partida da busca.
                double w = std::min(0.5, restartStreak / 5.0);

                if (penalizedCost < bestCost) {
                    // Dive promissor: roda VND a partir do ponto inviável sem reparar.
                    // O VND só aceita moves que resultem em soluções viáveis, então migra
                    // naturalmente para uma região viável diferente da já explorada.
                    Allocation explored = neighborhoodSearch(matrix, diveAllocation, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario, searchMode, improvementCondition, ImprovementHeuristic, improvementMode);

                    if (validator.isFeasible(matrix, explored, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario, true)) {
                        if (explored.getCurrentCost() < bestCost) {
                            bestAllocation = explored;
                            bestCost = explored.getCurrentCost();
                            bestAllocation.setTimeToBest((nowMs() - instanceInitTime) / 1000.0);
                            restartStreak = 0;
                        }
                        currentAllocation = explored;
                    } else {
                        // VND não conseguiu voltar à viabilidade — reinicia com construtivo adaptativo
                        currentAllocation = adaptiveGreedyInitialSolution(matrix, alpha, matrix.getNumberOfTasks(), matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario, w);
                        restartStreak++;
                    }
                } else {
                    // Dive não promissor — reinicia com construtivo adaptativo
                    currentAllocation = adaptiveGreedyInitialSolution(matrix, alpha, matrix.getNumberOfTasks(), matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario, w);
                    restartStreak++;
                }
            }
        }

        return bestAllocation;
    }


    // ILS Clássico sem restart

    Allocation ILS_run(const InstanceMatrix& matrix, double alpha, int IT_MAX, double instanceInitTime, ProbabilityScenario pScenario, ImprovementHeuristic ImprovementHeuristic, SearchMode searchMode, ImprovementCondition improvementCondition, ImprovementMode improvementMode, PerturbationMode pertubationMode) {
        Allocation currentAllocation = greedyInitialSolution(matrix, alpha, matrix.getNumberOfTasks(), matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario);

        Allocation bestAllocation = neighborhoodSearch(matrix, currentAllocation, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario, searchMode, improvementCondition, ImprovementHeuristic, improvementMode);
        double bestCost = bestAllocation.getCurrentCost();
        bestAllocation.setTimeToBest((nowMs() - instanceInitTime) / 1000.0);

        for (int i = 0; i < IT_MAX; ++i) {
            Allocation perturbed = pertubation(currentAllocation, matrix, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario, i, IT_MAX, pertubationMode);

            Allocation improved = neighborhoodSearch(matrix, perturbed, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario, searchMode, improvementCondition, ImprovementHeuristic, improvementMode);

            if (improved.getCurrentCost() < bestCost) {
                bestAllocation = improved;
                bestCost = improved.getCurrentCost();
                bestAllocation.setTimeToBest((nowMs() - instanceInitTime) / 1000.0);
            }
        }

        return bestAllocation;
    }

    // Solução inicial usando probabilidade

    Allocation GreedyMinProbAllocation(const InstanceMatrix& matrix, ProbabilityScenario pScenario) {
        Allocation allocation;

        return ProbabilityBasedInitialSolution(matrix, 0.0, matrix.getNumberOfTasks(), matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario);
    }


private:
    Allocation pertubation(Allocation allocation, const InstanceMatrix& matrix, int Vmax, int Smax, double Pmax, ProbabilityScenario pScenario, int i, int IT_MAX, PerturbationMode pertubationMode) {
        if (pertubationMode == PerturbationMode::MOVE) {
            return pertubationMove(std::move(allocation), matrix, Vmax, Smax, Pmax, pScenario, i, IT_MAX);
        } else if (pertubationMode == PerturbationMode::SWAP) {
            return pertubationSwap(std::move(allocation), matrix, Vmax, Smax, Pmax, pScenario, i, IT_MAX);
        } else if (pertubationMode == PerturbationMode::INFEASIBLE_DIVE) {
            return pertubationInfeasible(std::move(allocation), matrix, Smax, i, IT_MAX);
        }
        return allocation;
    }

    // Perturbação que ignora intencionalmente a restrição de probabilidade.
    // Análogo ao Small_Perturbation do IGrAl (Caramia 2008): ao ficar preso num ótimo
    // local, força movimentos que violam Pmax para explorar regiões infeasíveis.
    // Apenas restrições duras de recurso (Smax, Vres) são mantidas.
    Allocation pertubationInfeasible(Allocation allocation, const InstanceMatrix& matrix, int Smax, int i, int IT_MAX) {
        int numberOfTasks    = matrix.getNumberOfTasks();
        int numberOfServices = matrix.getNumberOfServices();
        int limit = std::max(1, static_cast<int>(6 - (i / (IT_MAX * 1.0)) * 5));

        for (int j = 0; j < limit; ++j) {
            int taskId = RandomUtil::getRandomInt(0, numberOfTasks - 1);
            Task    task(taskId, matrix.getTaskConsumption(taskId));
            Service oldService(allocation.getServiceForTask(taskId));
            Service newService(RandomUtil::getRandomInt(0, numberOfServices - 1));

            allocation.replaceService(task, newService, matrix);

            // Desfaz apenas se violar restrições duras (Smax ou capacidade de recurso)
            if (allocation.getNumberOfEmployedServices() > Smax ||
                !allocation.respectsResourceRestriction(matrix)) {
                allocation.replaceService(task, oldService, matrix);
            }
        }
        return allocation;
    }

    Allocation pertubationSwap(Allocation allocation, const InstanceMatrix& matrix, int Vmax, int Smax, double Pmax, ProbabilityScenario pScenario, int i, int IT_MAX) {
        int numberOfTasks = matrix.getNumberOfTasks();
        int limit = static_cast<int>(6 - (i / (IT_MAX * 1.0)) * 5);

        for (int j = 0; j < limit;) {
            int taskId1 = RandomUtil::getRandomInt(0, numberOfTasks - 1);
            int taskId2 = RandomUtil::getRandomInt(0, numberOfTasks - 1);

            if (taskId1 == taskId2)
                continue;

            Task task1(taskId1, matrix.getTaskConsumption(taskId1));
            Task task2(taskId2, matrix.getTaskConsumption(taskId2));

            Service service1(allocation.getServiceForTask(taskId1));
            Service service2(allocation.getServiceForTask(taskId2));

            allocation.replaceService(task1, service2, matrix);
            allocation.replaceService(task2, service1, matrix);

            bool probRestrictionRequired = matrix.getServiceProb(service2.getServId()) > matrix.getServiceProb(service1.getServId()) ||
                                            matrix.getServiceProb(service1.getServId()) > matrix.getServiceProb(service2.getServId());

            if (!validator.isFeasible(matrix, allocation, Vmax, Smax, Pmax, pScenario, probRestrictionRequired)) {
                // Se a nova alocação não for viável, desfaz a mudança
                allocation.replaceService(task1, service1, matrix);
                allocation.replaceService(task2, service2, matrix);
            } else {
                j++;    // Se a nova alocação for viável, mantém a mudança
            }
        }

        return allocation;
    }


    Allocation pertubationMove(Allocation allocation, const InstanceMatrix& matrix, int Vmax, int Smax, double Pmax, ProbabilityScenario pScenario, int i, int IT_MAX) {
        int numberOfTasks = matrix.getNumberOfTasks();
        int numberOfServices = matrix.getNumberOfServices(); // todos os serviços disponíveis, não apenas os já em uso
        int limit = static_cast<int>(6 - (i / (IT_MAX * 1.0)) * 5);

        for (int j = 0; j < limit;) {
            int taskId = RandomUtil::getRandomInt(0, numberOfTasks - 1);
            Task task(taskId, matrix.getTaskConsumption(taskId));

            Service oldService(allocation.getServiceForTask(taskId));
            Service newService(RandomUtil::getRandomInt(0, numberOfServices - 1));


            allocation.replaceService(task, newService, matrix);

            bool probRestrictionRequired = matrix.getServiceProb(newService.getServId()) > matrix.getServiceProb(oldService.getServId());

            if (!validator.isFeasible(matrix, allocation, Vmax, Smax, Pmax, pScenario, probRestrictionRequired)) {
                // Se a nova alocação não for viável, desfaz a mudança
                allocation.replaceService(task, oldService, matrix);
            } else {
                j++;    // Se a nova alocação for viável, mantém a mudança
            }
        }


        return allocation;
    }


    // Escolher busca local ou VND
    Allocation neighborhoodSearch(
        const InstanceMatrix& matrix,
        Allocation currentAllocation,
        int Vmax, int Smax, double Pmax,
        ProbabilityScenario pScenario,
        SearchMode mode, ImprovementCondition improvementCondition, ImprovementHeuristic improvementHeuristic,
        ImprovementMode improvementMode) {

            if (mode == SearchMode::LOCAL_SEARCH) {
                if (improvementHeuristic == ImprovementHeuristic::COST_IMPROVEMENT)
                    GenericSearcher().costImprovement(currentAllocation, matrix, Vmax, Smax, Pmax, pScenario, improvementCondition, improvementMode);
            } else if (mode == SearchMode::VND) {
                return GenericSearcher().VND(currentAllocation, matrix, Vmax, Smax, Pmax, pScenario, improvementCondition);
            }

            return currentAllocation;

    }
    // Construtivo guloso adaptativo: desloca o critério de seleção de custo puro (w=0)
    // para probabilidade pura (w=1) conforme o número de reinicios consecutivos cresce.
    // Quando w=0 o comportamento é idêntico ao greedyInitialSolution original.
    Allocation adaptiveGreedyInitialSolution(const InstanceMatrix& matrix, double alpha,
            int numberOfTasks, int Vmax, int Smax, double Pmax,
            ProbabilityScenario pScenario, double w) {

        if (w <= 0.0)
            return greedyInitialSolution(matrix, alpha, numberOfTasks, Vmax, Smax, Pmax, pScenario);

        Allocation allocation;
        vector<int> tasksToAllocate;
        for (int i = 0; i < numberOfTasks; ++i)
            tasksToAllocate.push_back(i);

        // Intervalo global de probabilidade para normalização [0,1]
        int nServices = matrix.getNumberOfServices();
        double minProb = std::numeric_limits<double>::max();
        double maxProb = std::numeric_limits<double>::lowest();
        for (int s = 0; s < nServices; ++s) {
            double p = matrix.getServiceProb(s);
            minProb = std::min(minProb, p);
            maxProb = std::max(maxProb, p);
        }
        double probRange = (maxProb - minProb) > 1e-9 ? (maxProb - minProb) : 1.0;

        int cont = 0;
        while (allocation.numberOfTasksAllocated() < numberOfTasks) {
            if (cont > 3 * numberOfTasks)
                return ProbabilityBasedInitialSolution(matrix, alpha, numberOfTasks, Vmax, Smax, Pmax, pScenario);

            int idx    = RandomUtil::getRandomInt(0, (int)tasksToAllocate.size() - 1);
            int taskId = tasksToAllocate[idx];

            double minCost  = matrix.getMinCostForTask(taskId);
            double maxCost  = matrix.getMaxCostForTask(taskId);
            double costRange = (maxCost - minCost) > 1e-9 ? (maxCost - minCost) : 1.0;

            // Score blended para cada serviço: menor é melhor.
            // score = (1-w)*custo_norm + w*prob_norm
            vector<std::pair<double, int>> scored;
            scored.reserve(nServices);
            for (int s = 0; s < nServices; ++s) {
                double costNorm = (matrix.getTaskCost(taskId, s) - minCost) / costRange;
                double probNorm = (matrix.getServiceProb(s)       - minProb) / probRange;
                scored.emplace_back((1.0 - w) * costNorm + w * probNorm, s);
            }
            std::sort(scored.begin(), scored.end());

            // RCL: serviços com score dentro do limiar alpha (mesmo mecanismo do GRASP original)
            double minScore   = scored.front().first;
            double maxScore   = scored.back().first;
            double threshold  = minScore + alpha * (maxScore - minScore);

            vector<Service> rcl;
            for (auto& [score, sId] : scored) {
                if (score > threshold) break;  // lista ordenada, pode parar cedo
                rcl.emplace_back(sId, matrix.getTaskCost(taskId, sId));
            }

            Service chosen = RandomUtil::getRandomService(rcl);
            Task    task(taskId, matrix.getTaskConsumption(taskId));

            tasksToAllocate.erase(tasksToAllocate.begin() + idx);
            allocation.addTask(task, chosen, matrix);

            if (!validator.isFeasible(matrix, allocation, Vmax, Smax, Pmax, pScenario, true)) {
                allocation.removeTask(task, matrix);
                tasksToAllocate.push_back(taskId);
            }

            cont++;
        }

        return allocation;
    }

    // Solução inicial usando o método construtivo guloso randomizado
    Allocation greedyInitialSolution(const InstanceMatrix& matrix, double alpha, int numberOfTasks, int Vmax, int Smax, double Pmax, ProbabilityScenario pScenario) {
        Allocation allocation;

        vector<int> tasksToAllocate;

        for (int i = 0; i < numberOfTasks; ++i)
            tasksToAllocate.push_back(i);

        int cont = 0;

        while (allocation.numberOfTasksAllocated() < numberOfTasks) {
            if (cont > 3 * numberOfTasks) {
                return ProbabilityBasedInitialSolution(matrix, alpha, numberOfTasks, Vmax, Smax, Pmax, pScenario);
            }

            int id = RandomUtil::getRandomInt(0, (int)tasksToAllocate.size() - 1);

            double minCost = matrix.getMinCostForTask(tasksToAllocate[id]);
            double maxCost = matrix.getMaxCostForTask(tasksToAllocate[id]);

            vector<Service> candidateServices = matrix.getServicesWithMaxCost(tasksToAllocate[id], minCost + alpha * (maxCost - minCost));

            Service chosen = RandomUtil::getRandomService(candidateServices);
            Task task(tasksToAllocate[id], matrix.getTaskConsumption(tasksToAllocate[id]));

            tasksToAllocate.erase(tasksToAllocate.begin() + id);
            allocation.addTask(task, chosen, matrix);

            if (!validator.isFeasible(matrix, allocation, Vmax, Smax, Pmax, pScenario, true)) {
                allocation.removeTask(task, matrix);
                tasksToAllocate.push_back(task.getTaskId());
            }

            cont ++;
        }
        

        return allocation;
    }


    // Solução inicial usando o método construtivo baseado em probabilidade de violação do SLA
    Allocation ProbabilityBasedInitialSolution(const InstanceMatrix& matrix, double alpha, int numberOfTasks, int Vmax, int Smax, double Pmax, ProbabilityScenario pScenario) {
        Allocation allocation;

        vector<int> tasksToAllocate;

        while (allocation.numberOfTasksAllocated() < numberOfTasks) {
            for (int i = 0; i < numberOfTasks; ++i) {
                Task task(i, matrix.getTaskConsumption(i));


                // Ordena os serviços com base na probabilidade de violação do SLA, do menor para o maior

                vector<int> orderedServices(matrix.getNumberOfServices());
                std::iota(orderedServices.begin(), orderedServices.end(), 0);
                std::sort(orderedServices.begin(), orderedServices.end(), [&](int s1, int s2) {
                    return matrix.getServiceProb(s1) < matrix.getServiceProb(s2);
                }); 


                // Tenta do mais seguro ao menos seguro até conseguir alocar


                while (!allocation.hasTask(task.getTaskId())) {
                    if (orderedServices.empty()) break;
                    Service service(orderedServices.front());

                    allocation.addTask(task, service, matrix);

                    if (!validator.isFeasible(matrix, allocation, Vmax, Smax, Pmax, pScenario, true)) {
                        allocation.removeTask(task, matrix);
                        orderedServices.erase(orderedServices.begin()); // Remove o serviço mais arriscado e tenta o próximo
                    }
                }



            }

            
        }
        

        return allocation;
    }
};