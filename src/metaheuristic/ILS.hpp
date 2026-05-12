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

        for (int i = 0; i < IT_MAX; ++i) {
            currentAllocation = pertubation(currentAllocation, matrix, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario, i, IT_MAX, pertubationMode);

            currentAllocation = neighborhoodSearch(matrix, currentAllocation, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario, searchMode, improvementCondition, ImprovementHeuristic, improvementMode);

            if (currentAllocation.getCurrentCost() < bestCost) {
                bestAllocation = currentAllocation;
                bestCost = currentAllocation.getCurrentCost();
                bestAllocation.setTimeToBest((nowMs() - instanceInitTime) / 1000.0);
                contNotImproved = 0;
            } else {
                contNotImproved++;
            }

            if (contNotImproved > IT_MAX / 10) {
                contNotImproved = 0;

                // Inspirado no IGrAl (Caramia 2008): antes de reiniciar, mergulha em soluções
                // inviáveis para escapar do ótimo local e tenta reparar a viabilidade.
                Allocation diveAllocation = pertubationInfeasible(currentAllocation, matrix, matrix.getSmax(), i, IT_MAX);

                if (repairFeasibility(diveAllocation, matrix, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario)) {
                    diveAllocation = neighborhoodSearch(matrix, diveAllocation, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario, searchMode, improvementCondition, ImprovementHeuristic, improvementMode);

                    if (diveAllocation.getCurrentCost() < bestCost) {
                        bestAllocation = diveAllocation;
                        bestCost = diveAllocation.getCurrentCost();
                        bestAllocation.setTimeToBest((nowMs() - instanceInitTime) / 1000.0);
                    }
                    currentAllocation = diveAllocation;
                } else {
                    // Repair falhou: restart greedy como fallback
                    currentAllocation = greedyInitialSolution(matrix, alpha, matrix.getNumberOfTasks(), matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario);
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

    // Repara viabilidade substituindo tarefas dos serviços com maior probabilidade de
    // violação por serviços mais seguros. Análogo ao Manage_Infeasibility do IGrAl.
    // Retorna true se a alocação se tornar viável, false se não for possível reparar.
    bool repairFeasibility(Allocation& allocation, const InstanceMatrix& matrix,
                           int Vmax, int Smax, double Pmax, ProbabilityScenario pScenario) {
        const int maxSteps = matrix.getNumberOfTasks();

        for (int step = 0; step < maxSteps; ++step) {
            if (validator.isFeasible(matrix, allocation, Vmax, Smax, Pmax, pScenario, true))
                return true;

            const vector<int>& alloc = allocation.getAllocation();

            // Identifica a tarefa alocada ao serviço de maior probabilidade de violação
            int   mostViolatingTask = -1;
            double highestProb      = -1.0;
            for (int t = 0; t < (int)alloc.size(); ++t) {
                int s = alloc[t];
                if (s < 0) continue;
                double p = matrix.getServiceProb(s);
                if (p > highestProb) {
                    highestProb       = p;
                    mostViolatingTask = t;
                }
            }
            if (mostViolatingTask < 0) break;

            // Coleta serviços com probabilidade menor que a do serviço atual,
            // ordenados por probabilidade crescente (mais seguros primeiro)
            int currentServId = alloc[mostViolatingTask];
            vector<int> saferServices;
            for (int s = 0; s < matrix.getNumberOfServices(); ++s) {
                if (s != currentServId && matrix.getServiceProb(s) < highestProb)
                    saferServices.push_back(s);
            }
            std::shuffle(saferServices.begin(), saferServices.end(), RandomUtil::engine());

            Task    task(mostViolatingTask, matrix.getTaskConsumption(mostViolatingTask));
            Service oldService(currentServId);
            bool    moved = false;

            for (int safeServId : saferServices) {
                allocation.replaceService(task, Service(safeServId), matrix);

                if (allocation.getNumberOfEmployedServices() <= Smax &&
                    allocation.respectsResourceRestriction(matrix)) {
                    moved = true;
                    break; // aceita o serviço mais seguro que respeita restrições duras
                }
                allocation.replaceService(task, oldService, matrix);
            }

            if (!moved) break; // nenhum serviço mais seguro disponível — reparo impossível
        }

        return validator.isFeasible(matrix, allocation, Vmax, Smax, Pmax, pScenario, true);
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