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

// Busca Local Guiada (GLS) aplicada às atribuições tarefa--serviço.
//
// Cada atribuição (tarefa t no serviço j) é uma característica e possui uma penalidade
// p[t][j]. A busca escolhe movimentos usando o objetivo aumentado
//
//     h(s) = custoReal(s) + lambda * somaDasPenalidadesPresentes(s).
//
// As penalidades ajudam a trajetória a abandonar combinações repetidas. Elas
// servem apenas para guiar a busca: a solução devolvida é sempre a de menor
// custo real encontrada e continua respeitando capacidade, Smax e SLA.
class GuidedLocalSearcher {
    SolutionValidator validator_;
    // penalties_[tarefa][serviço] registra quantas vezes a característica foi penalizada.
    std::vector<std::vector<int>> penalties_;

    // Na GFLS, cada serviço representa uma sub-vizinhança formada pelos MOVEs e
    // SWAPs que entram nele ou saem dele. Valor 1 significa que ela deve ser
    // examinada; valor 0 evita repetir uma varredura que já não encontrou melhora.
    std::vector<unsigned char> activeServiceNeighborhoods_;

    // Peso que converte a soma das penalidades para a escala do custo real.
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
        // O incumbente é comparado exclusivamente pelo objetivo original.
        if (candidate.getCurrentCost() < best.getCurrentCost() - 1e-9)
            best = candidate;
    }

    // Executa uma descida no objetivo aumentado h(s). Em cada iteração avalia
    // MOVE e SWAP, escolhe o melhor delta negativo e aplica apenas movimentos
    // viáveis. A descida termina quando não existe melhoria em h(s).
    bool guidedDescent(Allocation& current, Allocation& best,
                       const InstanceMatrix& matrix,
                       int Vmax, int Smax, double Pmax,
                       ProbabilityScenario pScenario, double deadlineMs) {
        const int numberOfTasks = matrix.getNumberOfTasks();
        const int numberOfServices = matrix.getNumberOfServices();
        bool appliedAnyMove = false;

        // Na primeira utilização, todas as sub-vizinhanças começam ativas.
        if (static_cast<int>(activeServiceNeighborhoods_.size()) != numberOfServices)
            activeServiceNeighborhoods_.assign(numberOfServices, 1);
        if (std::none_of(activeServiceNeighborhoods_.begin(),
                         activeServiceNeighborhoods_.end(),
                         [](unsigned char active) { return active != 0; }))
            return false;

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

            // Vizinhança MOVE: transfere uma tarefa para outro serviço.
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
                    if (!activeServiceNeighborhoods_[oldService]
                            && !activeServiceNeighborhoods_[newService])
                        continue;

                    const double deltaCost =
                        matrix.getTaskCost(taskId, newService)
                        - matrix.getTaskCost(taskId, oldService);
                    const double deltaPenalty =
                        penalties_[taskId][newService]
                        - penalties_[taskId][oldService];
                    // deltaGuided é a variação de h(s), não apenas do custo real.
                    const double deltaGuided = deltaCost + lambda_ * deltaPenalty;
                    if (deltaGuided >= bestDelta)
                        continue;

                    // Aplica temporariamente para validar as três restrições duras
                    // e desfaz logo depois; o movimento ainda não foi escolhido.
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

            // Vizinhança SWAP: troca os serviços de duas tarefas.
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
                    if (!activeServiceNeighborhoods_[service1]
                            && !activeServiceNeighborhoods_[service2])
                        continue;

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
                // Nenhuma sub-vizinhança ativa melhorou h(s): chegamos a um
                // mínimo local desta rodada e podemos desativar todas elas.
                std::fill(activeServiceNeighborhoods_.begin(),
                          activeServiceNeighborhoods_.end(), 0);
                break;
            }

            // Aplica definitivamente o melhor movimento e reativa somente os
            // serviços afetados, pois suas sub-vizinhanças acabaram de mudar.
            if (bestKind == MoveKind::MOVE) {
                const int oldService = current.getServiceForTask(bestTask1);
                current.replaceService(
                    Task(bestTask1, matrix.getTaskConsumption(bestTask1)),
                    Service(bestService), matrix);
                activeServiceNeighborhoods_[oldService] = 1;
                activeServiceNeighborhoods_[bestService] = 1;
            } else {
                const int service1 = current.getServiceForTask(bestTask1);
                const int service2 = current.getServiceForTask(bestTask2);
                current.replaceService(
                    Task(bestTask1, matrix.getTaskConsumption(bestTask1)),
                    Service(service2), matrix);
                current.replaceService(
                    Task(bestTask2, matrix.getTaskConsumption(bestTask2)),
                    Service(service1), matrix);
                activeServiceNeighborhoods_[service1] = 1;
                activeServiceNeighborhoods_[service2] = 1;
            }

            appliedAnyMove = true;
            updateBest(current, best);
        }

        return appliedAnyMove;
    }

    void penalizeMaximumUtilityFeatures(const Allocation& localMinimum,
                                        const InstanceMatrix& matrix) {
        // No mínimo local, a utilidade prioriza características caras e ainda pouco
        // penalizadas: utilidade(t,j) = custo(t,j) / (1 + p[t][j]).
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

        // Em caso de empate, todas as características de utilidade máxima recebem
        // penalidade. Seus serviços voltam a ficar ativos para a próxima descida.
        for (int taskId : selectedTasks) {
            const int serviceId = localMinimum.getServiceForTask(taskId);
            ++penalties_[taskId][serviceId];
            activeServiceNeighborhoods_[serviceId] = 1;
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

        // Inicializa o estado da GLS ou o recria quando muda o tamanho da instância.
        // Com preservePenalties=true, chamadas posteriores reutilizam o aprendizado.
        if (!preservePenalties || dimensionsChanged || penalties_.empty()) {
            penalties_.assign(numberOfTasks, std::vector<int>(numberOfServices, 0));
            // Escala clássica: alpha controla a influência relativa das penalidades.
            lambda_ = alpha * std::max(1.0, initial.getCurrentCost())
                / std::max(1, numberOfTasks);
        }
        // A busca local, a oscilação e a perturbação podem mudar a solução entre
        // chamadas. Por isso, os bits GFLS são reiniciados, embora as penalidades
        // continuem persistentes.
        activeServiceNeighborhoods_.assign(numberOfServices, 1);

        Allocation current = initial;
        Allocation best = initial;
        // Cada rodada desce até um mínimo local de h(s), penaliza as características de
        // maior utilidade e então inicia uma nova descida com a paisagem alterada.
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
