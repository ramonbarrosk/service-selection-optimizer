
#pragma once
#include <map>
#include <vector>
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

    // ───────────────────── Oscilação estratégica ─────────────────────
    // Proposta (2): relaxa a capacidade (Vres) para restrição SUAVE, otimizando
    // o objetivo penalizado  f = custo + λ·sobrecarga.  Smax e SLA seguem DURAS.
    // Isso permite atravessar a região inviável-por-capacidade para reequilibrar
    // — o movimento que a busca cost-only (estritamente viável) nunca faz, e que
    // as instâncias apertadas exigem. Em instâncias folgadas a sobrecarga é sempre
    // 0, então o passo vira uma descida de custo comum e não atrapalha.
    //
    // Recebe uma solução VIÁVEL e retorna true se encontrou outra viável mais
    // barata (atualizando `all`); caso contrário deixa `all` intacta.
    bool oscillationImprovement(Allocation& all, const InstanceMatrix& matrix,
                                int Vmax, int Smax, double Pmax,
                                ProbabilityScenario pScenario, int rounds = 12) {
        int Vres = matrix.getVres();
        double startCost = all.getCurrentCost();

        Allocation work = all;
        Allocation best = all;
        double bestCost = (overloadOf(work, Vres) == 0) ? work.getCurrentCost()
                                                        : std::numeric_limits<double>::max();

        // λ na escala dos custos: começa moderado e oscila para visitar a fronteira.
        double meanCost = 0; int n = matrix.getNumberOfTasks();
        for (int t = 0; t < n; ++t) meanCost += matrix.getMinCostForTask(t);
        meanCost /= std::max(1, n);
        double lambda = std::max(1.0, meanCost * 2.0);

        for (int r = 0; r < rounds; ++r) {
            penalizedDescentMove(work, matrix, Vres, Vmax, Smax, Pmax, pScenario, lambda);
            if (overloadOf(work, Vres) == 0) {
                if (work.getCurrentCost() < bestCost) { best = work; bestCost = work.getCurrentCost(); }
                lambda *= 0.6;                  // relaxa: deixa cruzar para inviável-barato
                if (lambda < 0.25) lambda = 0.25;
            } else {
                lambda *= 2.0;                  // aperta: empurra de volta para viável
            }
        }
        // descida final garantindo viabilidade
        penalizedDescentMove(work, matrix, Vres, Vmax, Smax, Pmax, pScenario, lambda * 8.0);
        if (overloadOf(work, Vres) == 0 && work.getCurrentCost() < bestCost) {
            best = work; bestCost = work.getCurrentCost();
        }

        if (bestCost < startCost - 1e-9) { all = best; return true; }
        return false;
    }

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

                    if (!stopped) {
                        for (int i = 0; i < init && !stopped; ++i)
                            stopped = tryMove(i);
                    }

                }

                if (bestFound && condition == ImprovementCondition::BEST_IMPROVEMENT) {
                    // Aplica o melhor movimento encontrado
                    locallyImproved = true;
                    globallyImproved = true;
                    all.replaceService(Task(bestTaskIdMove, matrix.getTaskConsumption(bestTaskIdMove)), Service(bestServiceIdMove), matrix);
                }
            } while (locallyImproved);

            return globallyImproved;
       } else {

            // MODO SWAP - tenta trocar o serviço de cada tarefa com o serviço de outra tarefa, buscando redução de custo, e aceita a primeira troca que melhorar a solução (First Improvement) ou a melhor troca encontrada (Best Improvement)

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

                    if (!stopped) {
                        for (int i = 0; i < init && !stopped; ++i)
                            stopped = trySwap(i);
                    }

                }


                // Best improvement: Aplica a melhor troca encontrada após avaliar todas as possibilidades

                if (bestFound && condition == ImprovementCondition::BEST_IMPROVEMENT) {
                    // Aplica a melhor troca encontrada
                    locallyImproved = true;
                    globallyImproved = true;
                    all.replaceService(Task(t1Id, matrix.getTaskConsumption(t1Id)), Service(s1Id), matrix);
                    all.replaceService(Task(t2Id, matrix.getTaskConsumption(t2Id)), Service(s2Id), matrix);
                }

            } while (locallyImproved);

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

private:
    // Sobrecarga total de capacidade (soma de quanto cada serviço passa de Vres).
    long overloadOf(const Allocation& a, int Vres) const {
        long ov = 0;
        for (int u : a.getResourcePerService())
            if (u > Vres) ov += (u - Vres);
        return ov;
    }

    // SLA só pode piorar se o novo serviço for mais arriscado que o antigo.
    // Quando piora, roda o DP de Poisson-binomial para checar Pmax.
    bool slaOkAfterMove(Allocation& a, const InstanceMatrix& m, int Vmax, double Pmax,
                        ProbabilityScenario pScenario, int oldServ, int newServ) {
        if (m.getServiceProb(newServ) <= m.getServiceProb(oldServ)) return true;
        return validator.computeViolationExcess(m, a, Vmax, Pmax, pScenario) <= 1e-12;
    }

    // Descida best-improvement no objetivo penalizado f = custo + λ·sobrecarga,
    // só por MOVE. Aceita mover para serviço sobrecarregado se o ganho líquido em f
    // for positivo; Smax e SLA continuam restrições duras.
    bool penalizedDescentMove(Allocation& a, const InstanceMatrix& m, int Vres,
                              int Vmax, int Smax, double Pmax, ProbabilityScenario pScenario,
                              double lambda) {
        bool any = false, improved = true;
        int n = m.getNumberOfTasks(), S = m.getNumberOfServices();
        while (improved) {
            improved = false;
            int bestT = -1, bestS = -1; double bestDelta = -1e-9;
            const auto& alloc = a.getAllocation();
            for (int t = 0; t < n; ++t) {
                int c = alloc[t]; if (c < 0) continue;
                int cons = m.getTaskConsumption(t);
                const auto& load = a.getResourcePerService();
                long usedC   = load[c];
                long ovC_old = std::max(0L, usedC - (long)Vres);
                long ovC_new = std::max(0L, usedC - cons - (long)Vres);
                for (int s = 0; s < S; ++s) {
                    if (s == c) continue;
                    double dCost = m.getTaskCost(t, s) - m.getTaskCost(t, c);
                    long usedS   = load[s];
                    long ovS_old = std::max(0L, usedS - (long)Vres);
                    long ovS_new = std::max(0L, usedS + cons - (long)Vres);
                    double dOver = (ovC_new - ovC_old) + (ovS_new - ovS_old);
                    double dF = dCost + lambda * dOver;
                    if (dF < bestDelta) {
                        // valida Smax e SLA aplicando o move temporariamente
                        Task task(t, cons); Service ns(s), cs(c);
                        a.replaceService(task, ns, m);
                        bool ok = a.getNumberOfEmployedServices() <= Smax &&
                                  slaOkAfterMove(a, m, Vmax, Pmax, pScenario, c, s);
                        a.replaceService(task, cs, m);
                        if (ok) { bestDelta = dF; bestT = t; bestS = s; }
                    }
                }
            }
            if (bestT >= 0) {
                a.replaceService(Task(bestT, m.getTaskConsumption(bestT)), Service(bestS), m);
                improved = true; any = true;
            }
        }
        return any;
    }
};
