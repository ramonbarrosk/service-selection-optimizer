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
    // Parâmetros fixos da GLS. Após cada chamada sem melhoria, o número de
    // rodadas dobra de 30 para 60 e depois para o teto de 120.
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

    // Fluxo principal do ILS: constrói uma solução, melhora-a e então alterna
    // perturbação e busca local. A melhor solução global é guardada separadamente
    // da solução corrente usada para continuar a caminhada.
    Allocation ILS_run(const InstanceMatrix& matrix, double alpha, int IT_MAX, double instanceInitTime, ProbabilityScenario pScenario, ImprovementHeuristic ImprovementHeuristic, SearchMode searchMode, ImprovementCondition improvementCondition, ImprovementMode improvementMode, PerturbationMode pertubationMode, double deadlineMs = std::numeric_limits<double>::infinity()) {
        const auto deadlineReached = [&]() {
            // O deadline usa relógio monotônico para não ser afetado por ajustes
            // no relógio do sistema durante a execução.
            return std::isfinite(deadlineMs) && nowMs() >= deadlineMs;
        };
        // Primeiro tenta o best-fit determinístico. Se ele não conseguir alocar
        // todas as tarefas, usa o construtivo guloso histórico como alternativa.
        bool bfOk = false;
        Allocation currentAllocation = bestFitInitialSolution(matrix, 0.0, bfOk);
        if (!bfOk)
            currentAllocation = greedyInitialSolution(matrix, alpha, matrix.getNumberOfTasks(), matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario);

        Allocation bestAllocation = neighborhoodSearch(matrix, currentAllocation, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario, searchMode, improvementCondition, ImprovementHeuristic, improvementMode);
        double bestCost = bestAllocation.getCurrentCost();
        currentAllocation = bestAllocation;
        bestAllocation.setTimeToBest((nowMs() - instanceInitTime) / 1000.0);

#ifdef ENABLE_GLS
        // A GLS só é chamada após um período de estagnação. O mesmo objeto é
        // mantido durante todo o ILS para preservar as penalidades aprendidas.
        int iterationsWithoutImprovement = 0;
        int consecutiveUnsuccessfulGlsCalls = 0;
        GuidedLocalSearcher guidedSearcher;
        const int glsStagnationThreshold = std::max(50, IT_MAX / 20);
#endif

        for (int i = 0; i < IT_MAX; ++i) {
            if (deadlineReached())
                break;

            Allocation perturbed = pertubation(currentAllocation, matrix, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario, i, IT_MAX, pertubationMode);

            // A busca de vizinhança combina descida por custo, FLS e oscilação.
            Allocation improved = neighborhoodSearch(matrix, perturbed, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario, searchMode, improvementCondition, ImprovementHeuristic, improvementMode);

            // A caminhada do ILS avança mesmo quando o candidato não supera a
            // melhor solução global. Isso permite explorar outras bacias de atração.
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
                // Chamadas consecutivas sem ganho recebem mais rodadas para
                // diversificar com maior intensidade: 30, 60 e no máximo 120.
                const int multiplier =
                    1 << std::min(consecutiveUnsuccessfulGlsCalls, 2);
                const int guidedRounds = std::min(
                    kGlsMaxRounds, kGlsRounds * multiplier);
                const double bestCostBeforeGls = bestCost;
                Allocation guided = guidedSearcher.improve(
                    currentAllocation, matrix,
                    matrix.getVmax(), matrix.getSmax(), matrix.getPmax(), pScenario,
                    kGlsAlpha, guidedRounds, true, deadlineMs);

                // A GLS devolve sua melhor solução pelo custo real. Em seguida,
                // aplicamos novamente a busca local e a oscilação para refiná-la.
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
        // Intensificação final: reaproveita as penalidades aprendidas pela GLS
        // para explorar uma última vez a região da melhor solução encontrada.
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
        // A perturbação afasta a solução do mínimo local antes da próxima descida.
        if (pertubationMode == PerturbationMode::MOVE) {
            return pertubationMove(std::move(allocation), matrix, Vmax, Smax, Pmax, pScenario, i, IT_MAX);
        } else if (pertubationMode == PerturbationMode::SWAP) {
            return pertubationSwap(std::move(allocation), matrix, Vmax, Smax, Pmax, pScenario, i, IT_MAX);
        }
        return allocation;
    }

    Allocation pertubationSwap(Allocation allocation, const InstanceMatrix& matrix, int Vmax, int Smax, double Pmax, ProbabilityScenario pScenario, int i, int IT_MAX) {
        int numberOfTasks = matrix.getNumberOfTasks();
        // A intensidade cai gradualmente de aproximadamente 6 para 1 troca.
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
        // No começo diversifica mais; perto do fim faz alterações menores.
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
                // Depois da descida por custo, a oscilação tenta reorganizar a
                // capacidade e escapar de mínimos locais estritamente viáveis.
                searcher.oscillationImprovement(currentAllocation, matrix, Vmax, Smax, Pmax, pScenario);
            } else if (mode == SearchMode::VND) {
                Allocation r = GenericSearcher().VND(currentAllocation, matrix, Vmax, Smax, Pmax, pScenario, improvementCondition);
                GenericSearcher().oscillationImprovement(r, matrix, Vmax, Smax, Pmax, pScenario);
                return r;
            }

            return currentAllocation;

    }
    // ───────── Construtivo best-fit decrescente + mais barato viável (proposta 1) ─────────
    //
    // Constrói uma solução inicial "empacotada" — a estrutura que a oscilação (proposta 2)
    // precisa para render nas instâncias apertadas. A ideia tem dois passos:
    //
    //   1. "decreasing": aloca as tarefas MAIS PESADAS (maior consumo de recurso) primeiro.
    //      É a sabedoria do bin-packing First-Fit-Decreasing: encaixe as "pedras grandes"
    //      antes, senão elas não cabem em lugar nenhum no fim.
    //   2. "mais barato viável": para cada tarefa, escolhe o serviço MAIS BARATO que mantém a
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
        // O validador recebe o cenário por referência não constante.
        ProbabilityScenario scenario = ProbabilityScenario::Ps;

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
            //   - anexa os demais serviços depois, como alternativa, ainda em ordem de custo.
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
