
#pragma once
#include <map>
#include <vector>
#include <deque>
#include <climits>
#include <limits>
#include <algorithm>
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

    // ───────────────────────── Oscilação estratégica (proposta 2) ─────────────────────────
    //
    // A busca somente por custo (estritamente viável) fica presa em mínimos locais nas instâncias
    // apertadas: para baratear é preciso ÀS VEZES piorar o custo agora — mover uma tarefa
    // para um serviço mais caro que LIBERA capacidade num serviço lotado — para consolidar e
    // eliminar um serviço depois. Ela nunca dá esse passo porque ele piora o custo no momento.
    //
    // A oscilação resolve isso relaxando a capacidade (Vres) para restrição SUAVE: em vez de
    // proibir o excesso, penaliza-o no objetivo
    //
    //         f = custo + λ · sobrecarga_total_de_capacidade
    //
    // e faz λ OSCILAR (daí o nome): quando a solução está viável, reduz λ (deixa a busca
    // "cruzar" para regiões inviáveis-mas-baratas); quando está inviável, aumenta λ (empurra
    // de volta para a viabilidade). Assim ela atravessa a barreira de capacidade e reequilibra.
    // Smax e SLA continuam restrições DURAS. Em instâncias folgadas a sobrecarga é sempre 0,
    // então o passo vira uma descida de custo comum e é inócuo.
    //
    // Recebe uma solução VIÁVEL; devolve verdadeiro e atualiza `solution` se encontrou outra
    // solução viável mais barata; caso contrário, deixa `solution` intacta.
    bool oscillationImprovement(Allocation& solution, const InstanceMatrix& matrix,
                                int Vmax, int Smax, double Pmax,
                                ProbabilityScenario pScenario, int oscillationRounds = 12) {
        const int Vres = matrix.getVres();
        const double startingCost = solution.getCurrentCost();

        Allocation candidate = solution;      // solução de trabalho (pode ficar inviável)
        Allocation bestFeasible = solution;   // melhor solução VIÁVEL já vista
        double bestFeasibleCost = (totalCapacityOverload(candidate, Vres) == 0)
                                      ? candidate.getCurrentCost()
                                      : std::numeric_limits<double>::max();

        // λ na escala dos custos das tarefas. Começa moderado e oscila (abaixo) para
        // visitar a fronteira entre viável e inviável.
        double meanMinCost = 0;
        const int numberOfTasks = matrix.getNumberOfTasks();
        for (int t = 0; t < numberOfTasks; ++t) meanMinCost += matrix.getMinCostForTask(t);
        meanMinCost /= std::max(1, numberOfTasks);
        double lambda = std::max(1.0, meanMinCost * 2.0);

        for (int round = 0; round < oscillationRounds; ++round) {
            descendOnPenalizedObjective(candidate, matrix, Vres, Vmax, Smax, Pmax, pScenario, lambda);

            if (totalCapacityOverload(candidate, Vres) == 0) {          // caiu numa região viável
                if (candidate.getCurrentCost() < bestFeasibleCost) {
                    bestFeasible = candidate;
                    bestFeasibleCost = candidate.getCurrentCost();
                }
                lambda *= 0.6;                                          // relaxa: permite cruzar p/ inviável-barato
                if (lambda < 0.25) lambda = 0.25;
            } else {                                                    // está inviável
                lambda *= 2.0;                                          // aperta: empurra de volta p/ viável
            }
        }

        // Descida final com λ alto para garantir que a solução termine viável.
        descendOnPenalizedObjective(candidate, matrix, Vres, Vmax, Smax, Pmax, pScenario, lambda * 8.0);
        if (totalCapacityOverload(candidate, Vres) == 0 && candidate.getCurrentCost() < bestFeasibleCost) {
            bestFeasible = candidate;
            bestFeasibleCost = candidate.getCurrentCost();
        }

        if (bestFeasibleCost < startingCost - 1e-9) {   // só aceita se melhorou de fato
            solution = bestFeasible;
            return true;
        }
        return false;
    }

    // FIRST_IMPROVEMENT usa a ativação de sub-vizinhanças da FLS. Já
    // BEST_IMPROVEMENT mantém a varredura exaustiva de bestMove/bestSwap.
    bool costImprovement(Allocation& all, const InstanceMatrix& matrix, int Vmax, int Smax, double Pmax, ProbabilityScenario pScenario, ImprovementCondition condition, ImprovementMode mode) {
        if (mode == ImprovementMode::MOVE) {
            if (condition == ImprovementCondition::FIRST_IMPROVEMENT)
                return flsMove(all, matrix, Vmax, Smax, Pmax, pScenario);
            return bestMove(all, matrix, Vmax, Smax, Pmax, pScenario);
        } else {
            if (condition == ImprovementCondition::FIRST_IMPROVEMENT)
                return flsSwap(all, matrix, Vmax, Smax, Pmax, pScenario);
            return bestSwap(all, matrix, Vmax, Smax, Pmax, pScenario);
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

private:
    // ───────────────────────── FLS: sub-vizinhanças com bit de ativação ─────────────────────────
    //
    // Índice reverso serviço -> tarefas nele alocadas. Não vive em Allocation (que é copiada por
    // valor em todo lugar e a maioria dos usos não precisa disso); é local a cada chamada de FLS
    // e mantido incrementalmente no mesmo estilo que Allocation::replaceService já usa (decrementa
    // do antigo, incrementa no novo), só que para listas em vez de contadores.
    struct ServiceTaskIndex {
        vector<vector<int>> tasksInService;  // serviceId -> lista de taskIds nele
        vector<int> posInList;               // taskId -> índice de taskId dentro de tasksInService[seu serviço]

        void build(const vector<int>& allocation, int numberOfServices) {
            tasksInService.assign(numberOfServices, {});
            posInList.assign(allocation.size(), -1);
            for (int t = 0; t < (int)allocation.size(); ++t) {
                int s = allocation[t];
                if (s < 0) continue;
                posInList[t] = (int)tasksInService[s].size();
                tasksInService[s].push_back(t);
            }
        }

        // O(1) amortizado — remove por swap-com-o-último, mesma classe de complexidade das
        // atualizações incrementais que Allocation já faz para resourcePerService_/employedServices_.
        void moveTask(int taskId, int fromService, int toService) {
            auto& from = tasksInService[fromService];
            int pos = posInList[taskId];
            int last = from.back();
            from[pos] = last;
            posInList[last] = pos;
            from.pop_back();

            auto& to = tasksInService[toService];
            posInList[taskId] = (int)to.size();
            to.push_back(taskId);
        }
    };

    // Fila de sub-vizinhanças ativas (uma por tarefa). Seedada com todas as tarefas no início de
    // cada chamada FLS; drenada até esvaziar — equivalente a "enquanto existir bit ativo" do
    // pseudocódigo da Busca Local Rápida (Voudouris/Tsang, Handbook of Metaheuristics, cap. 11).
    struct ActiveQueue {
        vector<char> inQueue;   // taskId -> já está na fila (evita duplicar)
        std::deque<int> worklist;

        void seedAll(int numberOfTasks) {
            inQueue.assign(numberOfTasks, 1);
            worklist.clear();
            for (int t = 0; t < numberOfTasks; ++t) worklist.push_back(t);
        }

        void activate(int taskId) {
            if (!inQueue[taskId]) {
                inQueue[taskId] = 1;
                worklist.push_back(taskId);
            }
        }

        bool empty() const { return worklist.empty(); }

        int pop() {
            int taskId = worklist.front();
            worklist.pop_front();
            inQueue[taskId] = 0;
            return taskId;
        }
    };

    // MODO MOVE, primeira melhoria, com FLS: em vez de reexaminar todas as tarefas a cada
    // passada, mantém uma fila de tarefas "ativas". Ao aceitar um movimento de taskId de
    // currentServId para acceptedTarget, reativa taskId (que acaba entrando na lista de
    // acceptedTarget) e todas as outras tarefas atualmente nos dois serviços cuja carga mudou —
    // são as únicas cujo resultado de respectsResourceRestriction/Smax pode ter mudado desde a
    // última vez que foram tentadas. A checagem de viabilidade em si (validator.isFeasible) é
    // chamada exatamente como em firstMoveFullScan — FLS só muda quais pares são propostos e em
    // que ordem, nunca a lógica de viabilidade/melhora.
    bool flsMove(Allocation& all, const InstanceMatrix& matrix, int Vmax, int Smax, double Pmax, ProbabilityScenario pScenario) {
        bool globallyImproved = false;
        const int numberOfTasks = matrix.getNumberOfTasks();
        const int numberOfServices = matrix.getNumberOfServices();

        ServiceTaskIndex svcIndex;
        svcIndex.build(all.getAllocation(), numberOfServices);

        ActiveQueue queue;
        queue.seedAll(numberOfTasks);

        while (!queue.empty()) {
            int taskId = queue.pop();
            int currentServId = all.getServiceForTask(taskId);
            if (currentServId < 0) continue;

            int init = RandomUtil::getRandomInt(0, numberOfServices - 1);
            int acceptedTarget = -1;

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
                    acceptedTarget = i;
                    return true;
                } else {
                    all.replaceService(task, currentService, matrix);
                }

                return false;
            };

            bool stopped = false;

            for (int i = init; i < numberOfServices && !stopped; ++i)
                stopped = tryMove(i);

            if (!stopped) {
                for (int i = 0; i < init && !stopped; ++i)
                    stopped = tryMove(i);
            }

            if (acceptedTarget >= 0) {
                globallyImproved = true;
                svcIndex.moveTask(taskId, currentServId, acceptedTarget);

                // Espalha ativação: as duas listas já refletem o estado pós-movimento, então o
                // próprio taskId (agora em acceptedTarget) é reativado por este mesmo laço.
                for (int t : svcIndex.tasksInService[currentServId])  queue.activate(t);
                for (int t : svcIndex.tasksInService[acceptedTarget]) queue.activate(t);
            }
        }

        return globallyImproved;
    }

    // MODO MOVE, primeira melhoria: varredura completa (sem sub-vizinhanças) — tenta mover
    // cada tarefa para um serviço diferente e aceita o primeiro movimento válido que melhora
    // o custo, seguindo para a próxima tarefa na mesma passada.
    bool firstMoveFullScan(Allocation& all, const InstanceMatrix& matrix, int Vmax, int Smax, double Pmax, ProbabilityScenario pScenario) {
        bool globallyImproved = false;
        bool locallyImproved;

        do {
            locallyImproved = false;

            const auto& alloc = all.getAllocation();
            for (int taskId = 0; taskId < (int)alloc.size(); ++taskId) {
               int currentServId = alloc[taskId];
               if (currentServId < 0) continue;
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
                        // Aceita imediatamente o primeiro movimento válido
                        locallyImproved = true;
                        globallyImproved = true;
                        return true;
                    } else {
                        // Movimento inviável — desfaz
                        all.replaceService(task, currentService, matrix);
                    }

                    return false;
               };

               bool stopped = false;

               for (int i = init; i < matrix.getNumberOfServices() && !stopped; ++i)
                    stopped = tryMove(i);

                if (!stopped) {
                    for (int i = 0; i < init && !stopped; ++i)
                        stopped = tryMove(i);
                }
            }
        } while (locallyImproved);

        return globallyImproved;
    }

    // MODO MOVE, melhor melhoria: avalia todos os movimentos possíveis na passada e aplica
    // apenas o de maior ganho de custo.
    bool bestMove(Allocation& all, const InstanceMatrix& matrix, int Vmax, int Smax, double Pmax, ProbabilityScenario pScenario) {
        bool globallyImproved = false;
        bool locallyImproved;

        do {
            locallyImproved = false;

            int bestTaskIdMove = -1;
            int bestServiceIdMove = -1;
            int bestCostGain = -1;
            bool bestFound = false;

            const auto& alloc = all.getAllocation();
            for (int taskId = 0; taskId < (int)alloc.size(); ++taskId) {
               int currentServId = alloc[taskId];
               if (currentServId < 0) continue;
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
                        // Melhor melhoria: desfaz e guarda se for o melhor até agora
                        all.replaceService(task, currentService, matrix);

                        int costGain = matrix.getTaskCost(taskId, currentServId) - matrix.getTaskCost(taskId, i);

                        if (costGain > bestCostGain) {
                            bestCostGain = costGain;
                            bestTaskIdMove = taskId;
                            bestServiceIdMove = i;
                            bestFound = true;
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

                if (!stopped) {
                    for (int i = 0; i < init && !stopped; ++i)
                        stopped = tryMove(i);
                }
            }

            if (bestFound) {
                // Aplica o melhor movimento encontrado
                locallyImproved = true;
                globallyImproved = true;
                all.replaceService(Task(bestTaskIdMove, matrix.getTaskConsumption(bestTaskIdMove)), Service(bestServiceIdMove), matrix);
            }
        } while (locallyImproved);

        return globallyImproved;
    }

    // MODO SWAP, primeira melhoria, com FLS: mesma ideia de flsMove, mas cada movimento
    // aceito troca DOIS participantes entre dois serviços (currentTaskServId <-> partnerServId).
    // A checagem de SLA nunca roda para SWAP (permuta o multiset de probabilidades sem mudar
    // a distribuição de violação — ver comentário em firstSwapFullScan/isFeasible), então a
    // única viabilidade que pode mudar entre tentativas é Smax/capacidade dos dois serviços
    // envolvidos — reativa todas as tarefas atualmente neles, o que cobre os dois mutantes
    // (cada um termina dentro da lista do outro serviço).
    bool flsSwap(Allocation& all, const InstanceMatrix& matrix, int Vmax, int Smax, double Pmax, ProbabilityScenario pScenario) {
        bool globallyImproved = false;
        const int numberOfTasks = matrix.getNumberOfTasks();
        const int numberOfServices = matrix.getNumberOfServices();

        ServiceTaskIndex svcIndex;
        svcIndex.build(all.getAllocation(), numberOfServices);

        ActiveQueue queue;
        queue.seedAll(numberOfTasks);

        while (!queue.empty()) {
            int currentTaskId = queue.pop();
            int currentTaskServId = all.getServiceForTask(currentTaskId);
            if (currentTaskServId < 0) continue;

            int init = RandomUtil::getRandomInt(0, numberOfTasks - 1);
            int acceptedPartner = -1;
            int acceptedPartnerServId = -1;

            auto trySwap = [&](int i) -> bool {
                if (i == currentTaskId) return false;

                int randomTaskServId = all.getServiceForTask(i);
                if (randomTaskServId < 0) return false;

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
                    acceptedPartner = i;
                    acceptedPartnerServId = randomTaskServId;
                    return true;
                } else {
                    // Troca inviável — desfaz
                    all.replaceService(currentTask, currentService, matrix);
                    all.replaceService(randomTask, randomTaskService, matrix);
                }

                return false;
            };

            bool stopped = false;

            for (int i = init; i < numberOfTasks && !stopped; ++i)
                stopped = trySwap(i);

            if (!stopped) {
                for (int i = 0; i < init && !stopped; ++i)
                    stopped = trySwap(i);
            }

            if (acceptedPartner >= 0) {
                globallyImproved = true;
                // acceptedPartnerServId foi capturado ANTES das replaceService acima.
                svcIndex.moveTask(currentTaskId, currentTaskServId, acceptedPartnerServId);
                svcIndex.moveTask(acceptedPartner, acceptedPartnerServId, currentTaskServId);

                for (int t : svcIndex.tasksInService[currentTaskServId])    queue.activate(t);
                for (int t : svcIndex.tasksInService[acceptedPartnerServId]) queue.activate(t);
            }
        }

        return globallyImproved;
    }

    // MODO SWAP, primeira melhoria: varredura completa — tenta trocar o serviço de cada
    // tarefa com o de outra tarefa e aceita a primeira troca válida que melhora o custo.
    bool firstSwapFullScan(Allocation& all, const InstanceMatrix& matrix, int Vmax, int Smax, double Pmax, ProbabilityScenario pScenario) {
        bool globallyImproved = false;
        bool locallyImproved;

        do {
            locallyImproved = false;

            const auto& alloc = all.getAllocation();
            for (int currentTaskId = 0; currentTaskId < (int)alloc.size(); ++currentTaskId) {
                int currentTaskServId = alloc[currentTaskId];
                if (currentTaskServId < 0) continue;
                int init = RandomUtil::getRandomInt(0, matrix.getNumberOfTasks() - 1);

                auto trySwap = [&](int i) -> bool {
                    if (i == currentTaskId) return false;

                    int randomTaskServId = all.getServiceForTask(i);
                    if (randomTaskServId < 0) return false;

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
                        // Aceita imediatamente a primeira troca válida
                        locallyImproved = true;
                        globallyImproved = true;
                        return true;
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

                if (!stopped) {
                    for (int i = 0; i < init && !stopped; ++i)
                        stopped = trySwap(i);
                }
            }
        } while (locallyImproved);

        return globallyImproved;
    }

    // MODO SWAP, melhor melhoria: avalia todas as trocas possíveis na passada e aplica
    // apenas a de maior ganho de custo.
    bool bestSwap(Allocation& all, const InstanceMatrix& matrix, int Vmax, int Smax, double Pmax, ProbabilityScenario pScenario) {
        bool globallyImproved = false;
        bool locallyImproved;

        do {
            locallyImproved = false;

            int t1Id = -1, t2Id = -1, s1Id = -1, s2Id = -1;
            int bestCostGain = -1;
            bool bestFound = false;

            const auto& alloc = all.getAllocation();
            for (int currentTaskId = 0; currentTaskId < (int)alloc.size(); ++currentTaskId) {
                int currentTaskServId = alloc[currentTaskId];
                if (currentTaskServId < 0) continue;
                int init = RandomUtil::getRandomInt(0, matrix.getNumberOfTasks() - 1);

                auto trySwap = [&](int i) -> bool {
                    if (i == currentTaskId) return false;

                    int randomTaskServId = all.getServiceForTask(i);
                    if (randomTaskServId < 0) return false;

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
                        // Melhor melhoria: desfaz e guarda se for a melhor até agora
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

                if (!stopped) {
                    for (int i = 0; i < init && !stopped; ++i)
                        stopped = trySwap(i);
                }
            }

            // Melhor melhoria: aplica a melhor troca encontrada após avaliar todas as possibilidades
            if (bestFound) {
                locallyImproved = true;
                globallyImproved = true;
                all.replaceService(Task(t1Id, matrix.getTaskConsumption(t1Id)), Service(s1Id), matrix);
                all.replaceService(Task(t2Id, matrix.getTaskConsumption(t2Id)), Service(s2Id), matrix);
            }
        } while (locallyImproved);

        return globallyImproved;
    }

    // Sobrecarga total de capacidade: soma de quanto cada serviço ultrapassa Vres.
    // Zero significa que a solução respeita a restrição de capacidade.
    long totalCapacityOverload(const Allocation& allocation, int Vres) const {
        long overload = 0;
        for (int load : allocation.getResourcePerService())
            if (load > Vres) overload += (load - Vres);
        return overload;
    }

    // O SLA só pode PIORAR se o serviço de destino for mais arriscado que o de origem.
    // Nesse caso roda a DP de Poisson-binomial (custosa) para conferir o limite Pmax;
    // caso contrário a viabilidade de SLA está garantida sem recalcular nada.
    bool slaStaysFeasible(Allocation& allocation, const InstanceMatrix& matrix, int Vmax,
                          double Pmax, ProbabilityScenario pScenario,
                          int fromService, int toService) {
        if (matrix.getServiceProb(toService) <= matrix.getServiceProb(fromService))
            return true;
        return validator.computeViolationExcess(matrix, allocation, Vmax, Pmax, pScenario) <= 1e-12;
    }

    // Descida por melhor melhoria sobre o objetivo penalizado f = custo + λ·sobrecarga,
    // usando apenas movimentos MOVE (uma tarefa muda de serviço). A cada passada escolhe o
    // MOVE que mais reduz f e o aplica; repete até nenhum MOVE reduzir f. Aceita mover para
    // um serviço sobrecarregado se o ganho líquido em f compensar (é isso que permite
    // atravessar a inviabilidade-de-capacidade). Smax e SLA continuam DUROS: um candidato
    // que os violaria é descartado antes de ser aceito.
    bool descendOnPenalizedObjective(Allocation& allocation, const InstanceMatrix& matrix,
                                     int Vres, int Vmax, int Smax, double Pmax,
                                     ProbabilityScenario pScenario, double lambda) {
        const int numberOfTasks    = matrix.getNumberOfTasks();
        const int numberOfServices = matrix.getNumberOfServices();

        bool appliedAnyMove = false;
        bool improvedThisPass = true;
        while (improvedThisPass) {
            improvedThisPass = false;

            int bestTask = -1, bestService = -1;
            double bestDeltaF = -1e-9;   // só aceita reduções reais de f (delta < 0)

            const auto& taskToService = allocation.getAllocation();
            for (int taskId = 0; taskId < numberOfTasks; ++taskId) {
                int currentService = taskToService[taskId];
                if (currentService < 0) continue;
                int consumption = matrix.getTaskConsumption(taskId);

                const auto& serviceLoad = allocation.getResourcePerService();
                // Sobrecarga do serviço de ORIGEM antes/depois de tirar esta tarefa dele.
                long fromLoad          = serviceLoad[currentService];
                long fromOverloadNow   = std::max(0L, fromLoad - (long)Vres);
                long fromOverloadAfter = std::max(0L, fromLoad - consumption - (long)Vres);

                for (int targetService = 0; targetService < numberOfServices; ++targetService) {
                    if (targetService == currentService) continue;

                    double deltaCost = matrix.getTaskCost(taskId, targetService)
                                     - matrix.getTaskCost(taskId, currentService);

                    // Sobrecarga do serviço de DESTINO antes/depois de receber esta tarefa.
                    long toLoad          = serviceLoad[targetService];
                    long toOverloadNow   = std::max(0L, toLoad - (long)Vres);
                    long toOverloadAfter = std::max(0L, toLoad + consumption - (long)Vres);

                    double deltaOverload = (fromOverloadAfter - fromOverloadNow)
                                         + (toOverloadAfter - toOverloadNow);
                    double deltaF = deltaCost + lambda * deltaOverload;

                    if (deltaF < bestDeltaF) {
                        // Candidato reduz f: confirma Smax e SLA aplicando o MOVE
                        // temporariamente (e desfazendo em seguida).
                        Task task(taskId, consumption);
                        Service target(targetService), current(currentService);
                        allocation.replaceService(task, target, matrix);
                        bool hardConstraintsOk =
                            allocation.getNumberOfEmployedServices() <= Smax &&
                            slaStaysFeasible(allocation, matrix, Vmax, Pmax, pScenario,
                                             currentService, targetService);
                        allocation.replaceService(task, current, matrix);

                        if (hardConstraintsOk) {
                            bestDeltaF = deltaF;
                            bestTask = taskId;
                            bestService = targetService;
                        }
                    }
                }
            }

            if (bestTask >= 0) {
                allocation.replaceService(Task(bestTask, matrix.getTaskConsumption(bestTask)),
                                          Service(bestService), matrix);
                improvedThisPass = true;
                appliedAnyMove = true;
            }
        }
        return appliedAnyMove;
    }
};
