// Diagnóstico da instância difícil (padrão: 100).
//
// 1. Lê a solução ótima registrada no log do CPLEX e confirma custo/viabilidade.
// 2. Executa a GLS promovida pelo tempo pedido e guarda a melhor solução.
// 3. Compara as duas soluções: tarefas realocadas, serviços usados, folga de
//    capacidade e custo excedente por tarefa.
// 4. Testa se a melhor solução da GLS é ótimo local também de uma vizinhança
//    maior: o reempacotamento exato das tarefas de k serviços (k = 2, 3, 4).
//
// Variáveis: SSO_INSTANCE (100), SSO_TIME_SECONDS (30), SSO_REPACK_K (3),
// SSO_ALPHA (0.3).

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "Allocation.h"
#include "GuidedLocalSearcher.hpp"
#include "InstanceMatrix.hpp"
#include "ProbabilityScenario.h"
#include "RandomUtil.hpp"
#include "Service.h"
#include "SolutionValidator.hpp"
#include "Task.h"

namespace {

double nowMs() {
    using namespace std::chrono;
    return static_cast<double>(duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count());
}

InstanceMatrix readInstance(int id) {
    std::ifstream file("data/instances/Instance_10_10_" + std::to_string(id));
    bool firstLine = true;
    InstanceMatrix instance;
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream input(line);
        std::vector<std::string> t;
        std::string token;
        while (input >> token)
            t.push_back(token);
        if (t.empty())
            continue;
        if (firstLine) {
            instance = InstanceMatrix(std::stoi(t[2]), std::stoi(t[1]),
                                      std::stoi(t[3]), std::stoi(t[5]),
                                      std::stod(t[4]), std::stoi(t[6]));
            firstLine = false;
        } else if (t[0] == "r") {
            instance.setResourceConsumption(std::stoi(t[1]) - 1, std::stoi(t[2]));
        } else if (t[0] == "p") {
            instance.setSlaViolationProbability(std::stoi(t[1]) - 1, std::stod(t[2]));
        } else if (t[0] == "c") {
            instance.setTaskCost(std::stoi(t[2]) - 1, std::stoi(t[1]) - 1,
                                 std::stoi(t[3]));
        }
    }
    for (int s = 0; s < instance.getNumberOfServices(); ++s)
        instance.setServResourceCapacity(s, instance.getVres());
    return instance;
}

// Lê "Cost = X" e as linhas "(s,t) - s t" do log do CPLEX.
std::pair<int, std::vector<int>> readOptimum(int id, int numberOfTasks) {
    std::ifstream file("data/Log/Instance_10_10_" + std::to_string(id));
    std::vector<int> assignment(numberOfTasks, -1);
    int optimum = -1;
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("(s,t) - ", 0) == 0) {
            std::istringstream input(line.substr(8));
            int s, t;
            input >> s >> t;
            assignment[t - 1] = s - 1;
        }
        const auto pos = line.find("Cost = ");
        if (pos != std::string::npos)
            optimum = std::stoi(line.substr(pos + 7));
    }
    return {optimum, assignment};
}

Allocation fromAssignment(const std::vector<int>& assignment,
                          const InstanceMatrix& matrix) {
    Allocation allocation;
    for (int t = 0; t < (int)assignment.size(); ++t)
        allocation.addTask(Task(t, matrix.getTaskConsumption(t)),
                           Service(assignment[t]), matrix);
    return allocation;
}

Allocation probabilityInitialSolution(const InstanceMatrix& matrix) {
    SolutionValidator validator;
    ProbabilityScenario scenario = ProbabilityScenario::Ps;
    Allocation allocation;
    std::vector<int> services(matrix.getNumberOfServices());
    std::iota(services.begin(), services.end(), 0);
    std::stable_sort(services.begin(), services.end(), [&](int a, int b) {
        return matrix.getServiceProb(a) < matrix.getServiceProb(b);
    });
    for (int t = 0; t < matrix.getNumberOfTasks(); ++t) {
        Task task(t, matrix.getTaskConsumption(t));
        for (int s : services) {
            allocation.addTask(task, Service(s), matrix);
            if (validator.isFeasible(matrix, allocation, matrix.getVmax(),
                                     matrix.getSmax(), matrix.getPmax(),
                                     scenario, true))
                break;
            allocation.removeTask(task, matrix);
        }
    }
    return allocation;
}

bool feasible(const Allocation& solution, const InstanceMatrix& matrix) {
    SolutionValidator validator;
    ProbabilityScenario scenario = ProbabilityScenario::Ps;
    Allocation copy = solution;
    return copy.numberOfTasksAllocated() == matrix.getNumberOfTasks()
        && validator.isFeasible(matrix, copy, matrix.getVmax(), matrix.getSmax(),
                                matrix.getPmax(), scenario, true);
}

void describe(const std::string& name, const Allocation& a,
              const InstanceMatrix& matrix) {
    int atMin = 0;
    double excess = 0.0;
    for (int t = 0; t < matrix.getNumberOfTasks(); ++t) {
        const double e = matrix.getTaskCost(t, a.getServiceForTask(t))
            - matrix.getMinCostForTask(t);
        excess += e;
        atMin += e < 1e-9;
    }
    int slack = 0;
    int full = 0;
    for (int s = 0; s < matrix.getNumberOfServices(); ++s) {
        if (a.getTaskCountPerService()[s] == 0)
            continue;
        slack += matrix.getVres() - a.getResourcePerService()[s];
        full += a.getResourcePerService()[s] == matrix.getVres();
    }
    std::cout << std::left << std::setw(10) << name
              << " custo " << a.getCurrentCost()
              << " | viável " << feasible(a, matrix)
              << " | serviços " << a.getNumberOfEmployedServices()
              << " (cheios " << full << ", folga total " << slack << ")"
              << " | tarefas no custo mínimo " << atMin
              << " | excedente " << excess << '\n';
}

// Busca exata: redistribui as tarefas dos serviços em `pool` entre os próprios
// serviços do pool, sem exceder o número de serviços empregados original.
// Retorna true e aplica a melhoria se encontrar custo real menor e viável.
bool repackExact(Allocation& solution, const InstanceMatrix& matrix,
                 const std::vector<int>& pool,
                 long long nodeLimit = std::numeric_limits<long long>::max()) {
    std::vector<int> tasks;
    for (int t = 0; t < matrix.getNumberOfTasks(); ++t)
        if (std::find(pool.begin(), pool.end(), solution.getServiceForTask(t))
                != pool.end())
            tasks.push_back(t);
    if (tasks.empty())
        return false;
    std::sort(tasks.begin(), tasks.end(), [&](int a, int b) {
        return matrix.getTaskConsumption(a) > matrix.getTaskConsumption(b);
    });
    int usedBefore = 0;
    double costBefore = 0.0;
    for (int s : pool)
        usedBefore += solution.getTaskCountPerService()[s] > 0;
    for (int t : tasks)
        costBefore += matrix.getTaskCost(t, solution.getServiceForTask(t));

    const int n = tasks.size();
    std::vector<double> suffixMin(n + 1, 0.0);
    for (int i = n - 1; i >= 0; --i) {
        double best = 1e18;
        for (int s : pool)
            best = std::min<double>(best, matrix.getTaskCost(tasks[i], s));
        suffixMin[i] = suffixMin[i + 1] + best;
    }
    // Serviços em ordem crescente de custo para cada tarefa: soluções boas
    // aparecem cedo e a poda por custo fica mais forte.
    std::vector<std::vector<int>> order(n);
    std::vector<int> suffixLoad(n + 1, 0);
    for (int i = n - 1; i >= 0; --i) {
        order[i].resize(pool.size());
        std::iota(order[i].begin(), order[i].end(), 0);
        std::stable_sort(order[i].begin(), order[i].end(), [&](int a, int b) {
            return matrix.getTaskCost(tasks[i], pool[a])
                < matrix.getTaskCost(tasks[i], pool[b]);
        });
        suffixLoad[i] = suffixLoad[i + 1] + matrix.getTaskConsumption(tasks[i]);
    }
    std::vector<int> load(pool.size(), 0), count(pool.size(), 0), choice(n);
    std::vector<int> bestChoice;
    double bestCost = costBefore - 0.5;  // custos inteiros: exige melhora >= 1
    int used = 0;
    long long nodes = 0;
    std::function<void(int, double)> dfs = [&](int i, double cost) {
        if (cost + suffixMin[i] > bestCost || ++nodes > nodeLimit)
            return;
        if (i == n) {
            bestCost = cost;
            bestChoice = choice;
            return;
        }
        const int t = tasks[i];
        int freeCapacity = 0;
        for (int k = 0; k < (int)pool.size(); ++k)
            if (count[k] > 0 || used < usedBefore)
                freeCapacity += matrix.getVres() - load[k];
        if (freeCapacity < suffixLoad[i])
            return;
        for (int k : order[i]) {
            if (load[k] + matrix.getTaskConsumption(t) > matrix.getVres())
                continue;
            if (count[k] == 0 && used == usedBefore)
                continue;
            load[k] += matrix.getTaskConsumption(t);
            used += count[k]++ == 0;
            choice[i] = k;
            dfs(i + 1, cost + matrix.getTaskCost(t, pool[k]));
            used -= --count[k] == 0;
            load[k] -= matrix.getTaskConsumption(t);
        }
    };
    dfs(0, 0.0);
    if (bestChoice.empty())
        return false;
    Allocation candidate = solution;
    for (int i = 0; i < n; ++i)
        candidate.replaceService(Task(tasks[i], matrix.getTaskConsumption(tasks[i])),
                                 Service(pool[bestChoice[i]]), matrix);
    if (!feasible(candidate, matrix))
        return false;
    solution = candidate;
    return true;
}

// Aplica reempacotamentos com k serviços até não haver melhoria. O pool inclui
// serviços vazios (k-1 usados + 1 vazio) para permitir trocar um serviço.
int repackDescent(Allocation& solution, const InstanceMatrix& matrix, int k) {
    int improvements = 0;
    bool improved = true;
    while (improved) {
        improved = false;
        std::vector<int> usedServices, emptyServices;
        for (int s = 0; s < matrix.getNumberOfServices(); ++s)
            (solution.getTaskCountPerService()[s] ? usedServices : emptyServices)
                .push_back(s);
        std::vector<int> pool;
        std::function<bool(int, int)> combos = [&](int start, int left) {
            if (left == 0) {
                if (repackExact(solution, matrix, pool))
                    return true;
                for (int e : emptyServices) {
                    pool.push_back(e);
                    const bool ok = repackExact(solution, matrix, pool);
                    pool.pop_back();
                    if (ok)
                        return true;
                }
                return false;
            }
            for (int i = start; i < (int)usedServices.size(); ++i) {
                pool.push_back(usedServices[i]);
                const bool ok = combos(i + 1, left - 1);
                pool.pop_back();
                if (ok)
                    return true;
            }
            return false;
        };
        for (int size = 2; size <= k && !improved; ++size) {
            if (combos(0, size)) {
                improved = true;
                ++improvements;
                std::cout << "    reempacotamento k=" << size
                          << " -> custo " << solution.getCurrentCost() << '\n';
            }
        }
    }
    return improvements;
}

// Reparo exato: reinsere `freeTasks` (já removidas de `solution`) em qualquer
// serviço com capacidade residual, respeitando Smax, com custo total menor que
// `targetCost`. Como quase todas as tarefas precisam ficar no custo mínimo, o
// orçamento de excedente restringe cada tarefa a poucos serviços; a busca em
// profundidade atribui primeiro a tarefa com menos opções (falha primeiro) e
// para na primeira solução que melhora.
long long gRepairCalls = 0, gRepairLimitHits = 0, gOracleCovered = 0;
std::set<int> gOracleDestroy;

bool repairExact(Allocation& solution, const InstanceMatrix& matrix,
                 std::vector<int> freeTasks, double targetCost, long long nodeLimit) {
    const int nServices = matrix.getNumberOfServices();
    const int n = freeTasks.size();
    const double fixedCost = solution.getCurrentCost();
    double minSum = 0.0;
    for (int t : freeTasks)
        minSum += matrix.getMinCostForTask(t);
    // Orçamento de excedente: quanto o custo pode passar da soma dos mínimos.
    const double budget = targetCost - 1.0 - fixedCost - minSum;  // custos inteiros
    if (budget < -1e-9)
        return false;
    // Candidatos por tarefa: só serviços com excedente dentro do orçamento.
    std::vector<std::vector<int>> candidates(n);
    for (int i = 0; i < n; ++i) {
        const int t = freeTasks[i];
        for (int s = 0; s < nServices; ++s)
            if (matrix.getTaskCost(t, s) - matrix.getMinCostForTask(t) <= budget + 1e-9)
                candidates[i].push_back(s);
        std::stable_sort(candidates[i].begin(), candidates[i].end(), [&](int x, int y) {
            return matrix.getTaskCost(t, x) < matrix.getTaskCost(t, y);
        });
    }
    std::vector<int> load = solution.getResourcePerService();
    std::vector<int> count = solution.getTaskCountPerService();
    int employed = solution.getNumberOfEmployedServices();
    std::vector<int> choice(n, -1);
    long long nodes = 0;
    bool found = false;
    auto fits = [&](int i, int s) {
        return load[s] + matrix.getTaskConsumption(freeTasks[i]) <= matrix.getVres()
            && (count[s] > 0 || employed < matrix.getSmax());
    };
    // Falha primeiro: escolhe a tarefa livre com menos opções dentro do orçamento.
    std::function<void(int, double)> dfs = [&](int assigned, double excess) {
        if (found || ++nodes > nodeLimit)
            return;
        if (assigned == n) {
            found = true;
            return;
        }
        int pick = -1, pickOptions = std::numeric_limits<int>::max();
        for (int i = 0; i < n; ++i) {
            if (choice[i] >= 0)
                continue;
            int options = 0;
            for (int s : candidates[i])
                if (excess + matrix.getTaskCost(freeTasks[i], s)
                        - matrix.getMinCostForTask(freeTasks[i]) <= budget + 1e-9
                        && fits(i, s))
                    ++options;
            if (options == 0)
                return;
            if (options < pickOptions
                    || (options == pickOptions
                        && matrix.getTaskConsumption(freeTasks[i])
                            > matrix.getTaskConsumption(freeTasks[pick]))) {
                pick = i;
                pickOptions = options;
            }
        }
        const int t = freeTasks[pick];
        for (int s : candidates[pick]) {
            const double extra = matrix.getTaskCost(t, s) - matrix.getMinCostForTask(t);
            if (excess + extra > budget + 1e-9)
                break;
            if (!fits(pick, s))
                continue;
            load[s] += matrix.getTaskConsumption(t);
            employed += count[s]++ == 0;
            choice[pick] = s;
            dfs(assigned + 1, excess + extra);
            if (found)
                return;
            choice[pick] = -1;
            employed -= --count[s] == 0;
            load[s] -= matrix.getTaskConsumption(t);
        }
    };
    dfs(0, 0.0);
    ++gRepairCalls;
    gRepairLimitHits += nodes > nodeLimit;
    if (!found)
        return false;
    Allocation candidate = solution;
    for (int i = 0; i < n; ++i)
        candidate.addTask(Task(freeTasks[i], matrix.getTaskConsumption(freeTasks[i])),
                          Service(choice[i]), matrix);
    if (!feasible(candidate, matrix))
        return false;
    solution = candidate;
    return true;
}

// LNS de destruição e reparo: parte de uma tarefa com custo acima do mínimo,
// esvazia o serviço dela e, em cadeia, serviços empregados onde as tarefas
// já liberadas teriam custo mínimo, até `destroyServices` serviços. Depois
// reinsere todas as tarefas liberadas de forma exata em qualquer serviço.
int destroyRepairLns(Allocation& solution, const InstanceMatrix& matrix,
                     int destroyServices, long long nodeLimit, double deadlineMs) {
    int improvements = 0;
    const int nTasks = matrix.getNumberOfTasks();
    const int nServices = matrix.getNumberOfServices();
    for (long long it = 0; nowMs() < deadlineMs; ++it) {
        std::vector<int> excess;
        for (int t = 0; t < nTasks; ++t)
            if (matrix.getTaskCost(t, solution.getServiceForTask(t))
                    > matrix.getMinCostForTask(t) + 1e-9)
                excess.push_back(t);
        if (excess.empty())
            break;
        std::vector<int> destroyed = {
            solution.getServiceForTask(excess[RandomUtil::getRandomNumber(excess.size())])};
        std::vector<int> freed;
        for (std::size_t next = 0; next < destroyed.size(); ++next) {
            for (int t = 0; t < nTasks; ++t)
                if (solution.getServiceForTask(t) == destroyed[next])
                    freed.push_back(t);
            if ((int)destroyed.size() >= destroyServices)
                continue;
            // Âncora: serviço (vazio ou não) de custo mínimo para uma tarefa
            // liberada. Destroem-se os serviços empregados que guardam outras
            // tarefas baratas na âncora, para que ela possa ser aberta/repovoada.
            const int pivot = freed[RandomUtil::getRandomNumber(freed.size())];
            std::vector<int> anchors;
            for (int s = 0; s < nServices; ++s)
                if (matrix.getTaskCost(pivot, s) <= matrix.getMinCostForTask(pivot) + 1e-9)
                    anchors.push_back(s);
            const int anchor = anchors[RandomUtil::getRandomNumber(anchors.size())];
            std::vector<int> holders;
            for (int t = 0; t < nTasks; ++t) {
                const int s = solution.getServiceForTask(t);
                if (matrix.getTaskCost(t, anchor) <= matrix.getMinCostForTask(t) + 1e-9
                        && std::find(destroyed.begin(), destroyed.end(), s) == destroyed.end()
                        && std::find(holders.begin(), holders.end(), s) == holders.end())
                    holders.push_back(s);
            }
            if (solution.getTaskCountPerService()[anchor] > 0
                    && std::find(destroyed.begin(), destroyed.end(), anchor) == destroyed.end()
                    && std::find(holders.begin(), holders.end(), anchor) == holders.end())
                holders.push_back(anchor);
            std::shuffle(holders.begin(), holders.end(), RandomUtil::engine());
            for (int h : holders)
                if ((int)destroyed.size() < destroyServices)
                    destroyed.push_back(h);
            if (holders.empty() && (int)destroyed.size() < destroyServices) {
                std::vector<int> others;
                for (int s = 0; s < nServices; ++s)
                    if (solution.getTaskCountPerService()[s] > 0
                            && std::find(destroyed.begin(), destroyed.end(), s) == destroyed.end())
                        others.push_back(s);
                destroyed.push_back(others[RandomUtil::getRandomNumber(others.size())]);
            }
        }
        gOracleCovered += !gOracleDestroy.empty() && std::all_of(
            gOracleDestroy.begin(), gOracleDestroy.end(), [&](int o) {
                return std::find(destroyed.begin(), destroyed.end(), o) != destroyed.end();
            });
        Allocation partial = solution;
        for (int t : freed)
            partial.removeTask(Task(t, matrix.getTaskConsumption(t)), matrix);
        if (repairExact(partial, matrix, freed, solution.getCurrentCost(), nodeLimit)) {
            solution = partial;
            ++improvements;
            std::cout << "    LNS it " << it << " (" << destroyed.size() << " serviços, "
                      << freed.size() << " tarefas) -> custo "
                      << solution.getCurrentCost() << '\n';
        }
    }
    return improvements;
}

}  // namespace

int main() {
    const int id = std::getenv("SSO_INSTANCE") ? std::atoi(std::getenv("SSO_INSTANCE")) : 100;
    const double seconds = std::getenv("SSO_TIME_SECONDS")
        ? std::atof(std::getenv("SSO_TIME_SECONDS")) : 30.0;
    const int repackK = std::getenv("SSO_REPACK_K") ? std::atoi(std::getenv("SSO_REPACK_K")) : 3;
    const double alpha = std::getenv("SSO_ALPHA") ? std::atof(std::getenv("SSO_ALPHA")) : 0.3;

    InstanceMatrix matrix = readInstance(id);
    const auto [optimum, optimalAssignment] =
        readOptimum(id, matrix.getNumberOfTasks());
    const Allocation optimal = fromAssignment(optimalAssignment, matrix);

    int consumption = 0;
    double lowerBound = 0.0;
    for (int t = 0; t < matrix.getNumberOfTasks(); ++t) {
        consumption += matrix.getTaskConsumption(t);
        lowerBound += matrix.getMinCostForTask(t);
    }
    std::cout << "Instância " << id << " | |T|=" << matrix.getNumberOfTasks()
              << " |S|=" << matrix.getNumberOfServices()
              << " Smax=" << matrix.getSmax() << " Vres=" << matrix.getVres()
              << " Vmax=" << matrix.getVmax() << " Pmax=" << matrix.getPmax() << '\n'
              << "Consumo total " << consumption << " / capacidade máxima "
              << matrix.getSmax() * matrix.getVres() << " ("
              << std::fixed << std::setprecision(1)
              << 100.0 * consumption / (matrix.getSmax() * matrix.getVres())
              << "%), mínimo de serviços "
              << (consumption + matrix.getVres() - 1) / matrix.getVres()
              << " | soma dos custos mínimos " << lowerBound
              << " | ótimo do log " << optimum << "\n\n" << std::setprecision(2);

    // GLS promovida (mesmo laço do experimento de melhorias, sem oscilação).
    const Allocation initial = probabilityInitialSolution(matrix);
    GuidedLocalSearcher gls;
    ProbabilityScenario scenario = ProbabilityScenario::Ps;
    Allocation current = initial;
    Allocation best = initial;
    const double start = nowMs();
    const double deadline = start + seconds * 1000.0;
    std::cout << "Trajetória da GLS (" << seconds << " s):\n";
    while (nowMs() < deadline) {
        current = gls.improve(current, matrix, matrix.getVmax(), matrix.getSmax(),
                              matrix.getPmax(), scenario, alpha, 30, true, deadline);
        if (current.getCurrentCost() < best.getCurrentCost() - 1e-9) {
            best = current;
            std::cout << "  t=" << std::setw(8) << (nowMs() - start) / 1000.0
                      << " s  custo " << best.getCurrentCost()
                      << "  rodadas " << gls.stats().completedPenaltyRounds << '\n';
        }
        if (best.getCurrentCost() <= optimum)
            break;
    }
    std::cout << "  lambda " << gls.stats().lambda << " | rodadas totais "
              << gls.stats().completedPenaltyRounds << "\n\n";

    describe("inicial", initial, matrix);
    describe("GLS", best, matrix);
    describe("ótimo", optimal, matrix);

    int differentTasks = 0;
    std::set<int> glsServices, optServices;
    for (int t = 0; t < matrix.getNumberOfTasks(); ++t) {
        differentTasks += best.getServiceForTask(t) != optimal.getServiceForTask(t);
        glsServices.insert(best.getServiceForTask(t));
        optServices.insert(optimal.getServiceForTask(t));
    }
    int commonServices = 0;
    for (int s : glsServices)
        commonServices += optServices.count(s);
    std::cout << "\nDistância GLS -> ótimo: " << differentTasks
              << " tarefas em serviços diferentes | serviços em comum "
              << commonServices << " de " << optServices.size()
              << " | só na GLS " << glsServices.size() - commonServices << '\n';

    std::cout << "\nTarefas fora do custo mínimo:\n";
    for (int t = 0; t < matrix.getNumberOfTasks(); ++t) {
        const int g = matrix.getTaskCost(t, best.getServiceForTask(t));
        const int o = matrix.getTaskCost(t, optimal.getServiceForTask(t));
        const double m = matrix.getMinCostForTask(t);
        if (g > m || o > m)
            std::cout << "  tarefa " << t + 1 << " (r=" << matrix.getTaskConsumption(t)
                      << ", mín " << m << "): GLS custo " << g << " em s"
                      << best.getServiceForTask(t) + 1 << " | ótimo custo " << o
                      << " em s" << optimal.getServiceForTask(t) + 1 << '\n';
    }

    // Quantas tarefas a GLS teria de mover num só passo para alcançar o ótimo?
    // O ótimo é ótimo local do reempacotamento? E a solução da GLS?
    for (int k = 2; k <= repackK; ++k) {
        Allocation probe = best;
        std::cout << "\nReempacotamento exato k<=" << k << " a partir da GLS:\n";
        const double t0 = nowMs();
        const int improvements = repackDescent(probe, matrix, k);
        std::cout << "  melhorias " << improvements << " | custo final "
                  << probe.getCurrentCost() << " | viável " << feasible(probe, matrix)
                  << " | " << (nowMs() - t0) / 1000.0 << " s\n";
    }

    // Diagnóstico: o pool formado pelos serviços que diferem entre a GLS e o
    // ótimo basta para o reempacotamento exato alcançar o ótimo?
    {
        std::vector<int> pool;
        for (int t = 0; t < matrix.getNumberOfTasks(); ++t) {
            if (best.getServiceForTask(t) == optimal.getServiceForTask(t))
                continue;
            for (int s : {best.getServiceForTask(t), optimal.getServiceForTask(t)})
                if (std::find(pool.begin(), pool.end(), s) == pool.end())
                    pool.push_back(s);
        }
        Allocation probe = best;
        const double t0 = nowMs();
        const bool ok = repackExact(probe, matrix, pool, 500000000LL);
        std::cout << "\nPool das diferenças (" << pool.size() << " serviços): "
                  << (ok ? "melhorou" : "sem melhora") << " -> custo "
                  << probe.getCurrentCost() << " | "
                  << (nowMs() - t0) / 1000.0 << " s\n";

        // Mesmo teste com destruição/reparo: esvazia os serviços da GLS que
        // contêm tarefas mal posicionadas e reinsere em qualquer serviço.
        std::vector<int> freed;
        std::set<int> destroyed;
        for (int t = 0; t < matrix.getNumberOfTasks(); ++t)
            if (best.getServiceForTask(t) != optimal.getServiceForTask(t))
                destroyed.insert(best.getServiceForTask(t));
        for (int t = 0; t < matrix.getNumberOfTasks(); ++t)
            if (destroyed.count(best.getServiceForTask(t)))
                freed.push_back(t);
        for (long long limit : {200000LL, 5000000LL, 100000000LL}) {
            Allocation partial = best;
            for (int t : freed)
                partial.removeTask(Task(t, matrix.getTaskConsumption(t)), matrix);
            const double t1 = nowMs();
            const bool repaired = repairExact(partial, matrix, freed,
                                              best.getCurrentCost(), limit);
            std::cout << "Destruição oráculo (" << destroyed.size() << " serviços, "
                      << freed.size() << " tarefas, limite " << limit << "): "
                      << (repaired ? "custo " + std::to_string((int)partial.getCurrentCost())
                                   : std::string("sem melhora"))
                      << " | " << (nowMs() - t1) / 1000.0 << " s\n";
        }
        gOracleDestroy = destroyed;
        std::cout << "Serviços destruídos no oráculo:";
        for (int d : destroyed) {
            std::cout << " s" << d + 1 << "[";
            for (int t = 0; t < matrix.getNumberOfTasks(); ++t)
                if (best.getServiceForTask(t) == d)
                    std::cout << " t" << t + 1 << "->s" << optimal.getServiceForTask(t) + 1;
            std::cout << " ]";
        }
        std::cout << '\n';
    }

    const int lnsPool = std::getenv("SSO_LNS_POOL") ? std::atoi(std::getenv("SSO_LNS_POOL")) : 0;
    if (lnsPool > 0) {
        const double lnsSeconds = std::getenv("SSO_LNS_SECONDS")
            ? std::atof(std::getenv("SSO_LNS_SECONDS")) : 60.0;
        const long long nodeLimit = std::getenv("SSO_LNS_NODES")
            ? std::atoll(std::getenv("SSO_LNS_NODES")) : 2000000;
        RandomUtil::setSeed(std::getenv("SSO_LNS_SEED") ? std::atoi(std::getenv("SSO_LNS_SEED")) : 1);
        Allocation probe = best;
        std::cout << "\nLNS destruição/reparo (" << lnsPool << " serviços" << ", " << lnsSeconds
                  << " s, limite de nós " << nodeLimit << "):\n";
        const double t0 = nowMs();
        destroyRepairLns(probe, matrix, lnsPool, nodeLimit, t0 + lnsSeconds * 1000.0);
        std::cout << "  reparos " << gRepairCalls << " | limite de nós atingido "
                  << gRepairLimitHits << " | destruições que cobrem o oráculo "
                  << gOracleCovered << '\n';
        std::cout << "  custo final " << probe.getCurrentCost() << " | viável "
                  << feasible(probe, matrix) << " | "
                  << (nowMs() - t0) / 1000.0 << " s\n";
    }
    return 0;
}
