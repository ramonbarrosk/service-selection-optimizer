#pragma once
#include <map>
#include <vector>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <climits>
#include <cmath>
#include <limits>
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
#ifdef ENABLE_GLS
#include "GuidedLocalSearcher.hpp"
#endif
#include "PertubationMode.h"

class ILS {

    SolutionValidator validator;

#ifdef ENABLE_GLS
    static constexpr double kGlsAlpha = 0.3;
    static constexpr int kGlsRounds = 30;
    static constexpr int kGlsMaxRounds = 120;
#endif

    static double nowMs() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }

public:
    ILS() = default;

    // ILS with best-fit construction, FLS and strategic oscillation.
    Allocation ILS_run(const InstanceMatrix& matrix, double alpha, int IT_MAX, double instanceInitTime, ProbabilityScenario pScenario, ImprovementHeuristic ImprovementHeuristic, SearchMode searchMode, ImprovementCondition improvementCondition, ImprovementMode improvementMode, PerturbationMode pertubationMode, double deadlineMs = std::numeric_limits<double>::infinity()) {
        const auto deadlineReached = [&]() {
            return std::isfinite(deadlineMs) && nowMs() >= deadlineMs;
        };
        // Start from best-fit and fall back to the historical greedy constructor.
        bool bfOk = false;
        Allocation currentAllocation = bestFitInitialSolution(matrix, 0.0, bfOk);
        if (!bfOk)
            currentAllocation = greedyInitialSolution(matrix, alpha, matrix.getNumberOfTasks(), matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario);

        Allocation bestAllocation = neighborhoodSearch(matrix, currentAllocation, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario, searchMode, improvementCondition, ImprovementHeuristic, improvementMode);
        double bestCost = bestAllocation.getCurrentCost();
        currentAllocation = bestAllocation;
        bestAllocation.setTimeToBest((nowMs() - instanceInitTime) / 1000.0);

#ifdef ENABLE_GLS
        int iterationsWithoutImprovement = 0;
        int consecutiveUnsuccessfulGlsCalls = 0;
        GuidedLocalSearcher guidedSearcher;
        const int glsStagnationThreshold = std::max(50, IT_MAX / 20);
#endif

        for (int i = 0; i < IT_MAX; ++i) {
            if (deadlineReached())
                break;

            Allocation perturbed = pertubation(currentAllocation, matrix, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario, i, IT_MAX, pertubationMode);

            Allocation improved = neighborhoodSearch(matrix, perturbed, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario, searchMode, improvementCondition, ImprovementHeuristic, improvementMode);

            // Let the ILS walk advance through different basins even when the
            // candidate does not improve the global best.
            currentAllocation = improved;

            if (improved.getCurrentCost() < bestCost) {
                bestAllocation = improved;
                bestCost = improved.getCurrentCost();
                bestAllocation.setTimeToBest((nowMs() - instanceInitTime) / 1000.0);
#ifdef ENABLE_GLS
                iterationsWithoutImprovement = 0;
                consecutiveUnsuccessfulGlsCalls = 0;
#endif
            } else {
#ifdef ENABLE_GLS
                ++iterationsWithoutImprovement;
#endif
            }

#ifdef ENABLE_GLS
            if (iterationsWithoutImprovement >= glsStagnationThreshold
                    && !deadlineReached()) {
                const int multiplier =
                    1 << std::min(consecutiveUnsuccessfulGlsCalls, 2);
                const int guidedRounds = std::min(
                    kGlsMaxRounds, kGlsRounds * multiplier);
                const double bestCostBeforeGls = bestCost;
                Allocation guided = guidedSearcher.improve(
                    currentAllocation, matrix,
                    matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario,
                    kGlsAlpha, guidedRounds, true, deadlineMs);

                // Polish the best real-cost solution returned by GLS using the
                // established local-search and strategic-oscillation pipeline.
                if (!deadlineReached()) {
                    guided = neighborhoodSearch(
                        matrix, guided, matrix.getVmax(), matrix.getSmax(),
                        matrix.getPmax(), pScenario, searchMode,
                        improvementCondition, ImprovementHeuristic, improvementMode);
                }
                currentAllocation = guided;

                if (guided.getCurrentCost() < bestCost) {
                    bestAllocation = guided;
                    bestCost = guided.getCurrentCost();
                    bestAllocation.setTimeToBest(
                        (nowMs() - instanceInitTime) / 1000.0);
                }

                if (bestCost < bestCostBeforeGls - 1e-9)
                    consecutiveUnsuccessfulGlsCalls = 0;
                else
                    consecutiveUnsuccessfulGlsCalls = std::min(
                        consecutiveUnsuccessfulGlsCalls + 1, 2);
                iterationsWithoutImprovement = 0;
            }
#endif
        }

#ifdef ENABLE_GLS
        // Intensify the best basin one last time with the learned penalties.
        const int finalGuidedRounds = std::min(
            kGlsMaxRounds,
            kGlsRounds * (1 << std::min(consecutiveUnsuccessfulGlsCalls, 2)));
        if (!deadlineReached()) {
            Allocation guidedBest = guidedSearcher.improve(
                bestAllocation, matrix,
                matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario,
                kGlsAlpha, finalGuidedRounds, true, deadlineMs);
            if (!deadlineReached()) {
                guidedBest = neighborhoodSearch(
                    matrix, guidedBest, matrix.getVmax(), matrix.getSmax(),
                    matrix.getPmax(), pScenario, searchMode, improvementCondition,
                    ImprovementHeuristic, improvementMode);
            }
            if (guidedBest.getCurrentCost() < bestCost) {
                bestAllocation = guidedBest;
                bestAllocation.setTimeToBest(
                    (nowMs() - instanceInitTime) / 1000.0);
            }
        }
#endif

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
                GenericSearcher searcher;
                if (improvementHeuristic == ImprovementHeuristic::COST_IMPROVEMENT)
                    searcher.costImprovement(currentAllocation, matrix, Vmax, Smax, Pmax, pScenario, improvementCondition, improvementMode);
                // Rebalance capacity after the cost-only descent.
                searcher.oscillationImprovement(currentAllocation, matrix, Vmax, Smax, Pmax, pScenario);
            } else if (mode == SearchMode::VND) {
                Allocation r = GenericSearcher().VND(currentAllocation, matrix, Vmax, Smax, Pmax, pScenario, improvementCondition);
                GenericSearcher().oscillationImprovement(r, matrix, Vmax, Smax, Pmax, pScenario);
                return r;
            }

            return currentAllocation;

    }
    // ───────────── Construtivo best-fit-decrescente + cheapest-feasible (proposta 1) ─────────────
    //
    // Constrói uma solução inicial "empacotada" — a estrutura que a oscilação (proposta 2)
    // precisa para render nas instâncias apertadas. A ideia tem dois passos:
    //
    //   1. "decreasing": aloca as tarefas MAIS PESADAS (maior consumo de recurso) primeiro.
    //      É a sabedoria do bin-packing First-Fit-Decreasing: encaixe as "pedras grandes"
    //      antes, senão elas não cabem em lugar nenhum no fim.
    //   2. "cheapest-feasible": para cada tarefa, escolhe o serviço MAIS BARATO que mantém a
    //      solução viável em TODAS as restrições (capacidade, Smax, SLA) — checa a viabilidade
    //      ANTES de fixar a alocação, em vez de alocar cego e reparar depois.
    //
    // Parâmetro `alpha` para diversificação GRASP/RCL:
    //   - alpha == 0 → determinístico: sempre tenta do serviço mais barato ao mais caro.
    //   - alpha  > 0 → entre os serviços "baratos o suficiente" (custo ≤ limiar RCL), tenta
    //                  numa ordem ALEATÓRIA, gerando soluções iniciais diferentes.
    //
    // `feasibleComplete` sai `false` se alguma tarefa não coube em nenhum serviço (instância
    // apertada demais para o construtivo) — o chamador então cai no guloso original.
    Allocation bestFitInitialSolution(const InstanceMatrix& matrix, double alpha, bool& feasibleComplete) {
        const int numberOfTasks    = matrix.getNumberOfTasks();
        const int numberOfServices = matrix.getNumberOfServices();
        const int Vmax = matrix.getVmax();
        const int Smax = matrix.getSmax();
        const double Pmax = matrix.getPmax();
        ProbabilityScenario scenario = ProbabilityScenario::Ps;   // isFeasible pede ref não-const

        Allocation allocation;

        // Passo 1: ordena as tarefas por consumo de recurso DECRESCENTE (pesadas primeiro).
        vector<int> tasksByConsumptionDesc(numberOfTasks);
        std::iota(tasksByConsumptionDesc.begin(), tasksByConsumptionDesc.end(), 0);
        std::sort(tasksByConsumptionDesc.begin(), tasksByConsumptionDesc.end(),
                  [&](int taskA, int taskB) {
                      return matrix.getTaskConsumption(taskA) > matrix.getTaskConsumption(taskB);
                  });

        feasibleComplete = true;
        for (int taskId : tasksByConsumptionDesc) {
            Task task(taskId, matrix.getTaskConsumption(taskId));

            // Serviços ordenados por custo crescente para esta tarefa: pares (custo, serviceId).
            vector<std::pair<int, int>> servicesByCost;
            servicesByCost.reserve(numberOfServices);
            for (int serviceId = 0; serviceId < numberOfServices; ++serviceId)
                servicesByCost.emplace_back(matrix.getTaskCost(taskId, serviceId), serviceId);
            std::sort(servicesByCost.begin(), servicesByCost.end());

            // Monta a ORDEM DE TENTATIVA (índices em servicesByCost):
            //   - Lista candidata RCL = serviços com custo ≤ limiar (todos, se alpha==0);
            //   - se alpha>0, embaralha a RCL para diversificar a construção;
            //   - anexa os demais serviços depois, como fallback, ainda em ordem de custo.
            const int cheapestCost = servicesByCost.front().first;
            const int priciestCost = servicesByCost.back().first;
            const int rclThreshold = cheapestCost + (int)(alpha * (priciestCost - cheapestCost));

            vector<int> tryOrder;
            for (size_t i = 0; i < servicesByCost.size(); ++i)
                if (alpha == 0.0 || servicesByCost[i].first <= rclThreshold)
                    tryOrder.push_back((int)i);
            if (alpha > 0.0)
                std::shuffle(tryOrder.begin(), tryOrder.end(), RandomUtil::engine());
            for (size_t i = 0; i < servicesByCost.size(); ++i)
                if (std::find(tryOrder.begin(), tryOrder.end(), (int)i) == tryOrder.end())
                    tryOrder.push_back((int)i);

            // Passo 2: atribui ao primeiro serviço (nessa ordem) que mantém tudo viável.
            bool assigned = false;
            for (int i : tryOrder) {
                Service service(servicesByCost[i].second, servicesByCost[i].first);
                allocation.addTask(task, service, matrix);
                if (validator.isFeasible(matrix, allocation, Vmax, Smax, Pmax, scenario, true)) {
                    assigned = true;
                    break;
                }
                allocation.removeTask(task, matrix);   // não coube: desfaz e tenta o próximo
            }

            if (!assigned) {                            // nenhum serviço acomodou a tarefa
                feasibleComplete = false;
                return allocation;
            }
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
