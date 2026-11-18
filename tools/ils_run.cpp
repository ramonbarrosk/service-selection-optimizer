// Porte fiel para C++ do ILS#3 de NASCIMENTO et al. (projeto Java de referência:
// metaheuristic/ILS.java, search/GenericSearcher.java, validator/SolutionValidator.java,
// util/RandomUtil.java e o laço de main/ArticleResult.java).
//
// O objetivo é separar o efeito da linguagem do efeito do método: o algoritmo é o
// mesmo do Java, inclusive nos detalhes que afetam a trajetória ou o custo:
//   - construção inicial gulosa pela menor probabilidade de violação, com o mesmo
//     bubble sort por tarefa e a verificação completa do SLA a cada tentativa;
//   - busca local de melhor melhoria na vizinhança MOVE, tarefas em ordem crescente
//     (a ordem de iteração do HashMap<Task, Service> do Java para n = 100), varredura
//     de serviços a partir de um deslocamento aleatório, só movimentos que reduzem o
//     custo, SLA verificado só quando o novo serviço tem probabilidade maior, maior
//     ganho estrito;
//   - verificação do SLA pela DP completa O(n · Vmax), com a mesma saída antecipada;
//   - perturbação MOVE com l = (int)(6 - 5 i / IT) movimentos aceitos;
//   - os sorteios de RandomUtil: nextInt(k + 1), com 0 e 1 mapeados para o índice 0;
//   - dois comportamentos do original preservados no porte (alternativa em keepbest):
//     a perturbação atua sobre o próprio objeto da melhor solução, e o ILS devolve o
//     último ótimo local; a perturbação MOVE sorteia um serviço para decidir a
//     verificação do SLA e outro para a troca;
//   - o laço externo de ArticleResult: chamadas de ILS com 10.000 iterações, repetidas
//     até o orçamento acabar ou uma chamada devolver o ótimo conhecido.
//
// Variáveis de ambiente (as mesmas do executável da GLS, quando existem):
//   SSO_INSTANCE_DIR, SSO_LOG_DIR, SSO_INSTANCES, SSO_REPETITIONS (padrão 20),
//   SSO_TIME_SECONDS (padrão 10), SSO_CSV, SSO_ILS_KEEPBEST (0/1, padrão 0),
//   SSO_ILS_ITERATIONS (padrão 10000).
//   SSO_ILS_INCREMENTAL e SSO_ILS_REPLACEMENT (0/1, padrão 0): a busca local passa a ser
//   a do ImprovedSearcher.java (fase 12), com avaliação incremental do SLA e/ou a
//   vizinhança de substituição de serviço. Desligadas, o porte é o ILS#3 original.
//   SSO_ILS_SINGLE_CALL=1: uma única chamada de ILS sem limite de tempo, para validar o
//   porte contra o Java com o mesmo número de iterações.
//
// CSV com as mesmas colunas de ArticleResult.java.

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "InstanceMatrix.hpp"
#include "InstanceReader.hpp"

namespace {

double nowMs() {
    using namespace std::chrono;
    return static_cast<double>(duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count());
}

std::mt19937_64& rng() {
    static std::mt19937_64 engine(std::random_device{}());
    return engine;
}

// RandomUtil.java: nextInt(k + 1) e o resultado 0 vira 0, os demais r viram r - 1.
int javaRandomIndex(int k) {
    std::uniform_int_distribution<int> dist(0, k);
    const int r = dist(rng());
    return r == 0 ? 0 : r - 1;
}

// Allocation.java com vetores no lugar dos HashMaps. A semântica é a mesma: custo
// acumulado, carga e número de tarefas por serviço, serviços empregados.
struct Allocation {
    std::vector<int> service;  // -1: tarefa ainda não alocada
    std::vector<int> load;
    std::vector<int> count;
    int employed = 0;
    int allocated = 0;
    double cost = 0.0;
    double timeToBest = 0.0;

    Allocation(int tasks, int services)
        : service(tasks, -1), load(services, 0), count(services, 0) {}

    void attach(int task, int s, const InstanceMatrix& m) {
        cost += m.getTaskCost(task, s);
        if (count[s]++ == 0)
            ++employed;
        load[s] += m.getTaskConsumption(task);
        service[task] = s;
    }

    void detach(int task, const InstanceMatrix& m) {
        const int s = service[task];
        cost -= m.getTaskCost(task, s);
        if (--count[s] == 0)
            --employed;
        load[s] -= m.getTaskConsumption(task);
        service[task] = -1;
    }

    void addTask(int task, int s, const InstanceMatrix& m) {
        attach(task, s, m);
        ++allocated;
    }

    void removeTask(int task, const InstanceMatrix& m) {
        detach(task, m);
        --allocated;
    }

    void replaceService(int task, int s, const InstanceMatrix& m) {
        detach(task, m);
        attach(task, s, m);
    }
};

// SolutionValidator.isFeasible, cenário Ps.
class Validator {
    std::vector<double> previous, current;

public:
    bool isFeasible(const Allocation& all, const InstanceMatrix& m, bool verifyProb) {
        if (all.employed > m.getSmax())
            return false;
        const int vres = m.getVres();
        for (int l : all.load)
            if (l > vres)
                return false;
        if (!verifyProb)
            return true;

        const int vmax = m.getVmax();
        const int n = m.getNumberOfTasks();
        const double pmax = m.getPmax();
        previous.assign(vmax + 1, 0.0);
        current.assign(vmax + 1, 0.0);
        for (int i = 0; i <= n; ++i) {
            double sum = 0.0;
            const double p = (i > 0 && all.service[i - 1] >= 0)
                ? m.getServiceProb(all.service[i - 1]) : 0.0;
            for (int k = 0; k <= vmax; ++k) {
                double value;
                if (i == 0 && k == 0)
                    value = 1.0;
                else if (k > i)
                    value = 0.0;
                else if (k == 0)
                    value = (1.0 - p) * previous[k];
                else
                    value = p * previous[k - 1] + (1.0 - p) * previous[k];
                current[k] = value;
                sum += value;
            }
            if (1.0 - sum > pmax)
                return false;
            std::swap(previous, current);
        }
        double notViolated = 0.0;
        for (int k = 0; k <= vmax; ++k)
            notViolated += previous[k];
        return 1.0 - notViolated <= pmax;
    }
};

class Ils {
    const InstanceMatrix& m;
    Validator validator;
    const bool keepBest;
    const bool incremental;
    const bool replacement;
    const int n;
    const int services;

    static constexpr double kSlaTolerance = 1e-12;
    static constexpr int kRebuildInterval = 32;

    // ImprovedSearcher.distribution: distribuição truncada do número de violações,
    // sem a tarefa excluded (-1: todas).
    std::vector<double> distribution(const Allocation& all, int excluded = -1) const {
        const int vmax = m.getVmax();
        std::vector<double> d(vmax + 1, 0.0);
        d[0] = 1.0;
        for (int t = 0; t < n; ++t) {
            if (t == excluded)
                continue;
            const double p = m.getServiceProb(all.service[t]);
            for (int v = vmax; v >= 0; --v)
                d[v] = d[v] * (1.0 - p) + (v > 0 ? d[v - 1] * p : 0.0);
        }
        return d;
    }

    static double violationProbability(const std::vector<double>& d) {
        double sum = 0.0;
        for (double value : d)
            sum += value;
        return std::min(1.0, std::max(0.0, 1.0 - sum));
    }

    // ImprovedSearcher.removeOne: retira da distribuição uma tarefa com probabilidade p.
    std::vector<double> removeOne(const std::vector<double>& d, double p,
                                  const Allocation& all, int task) const {
        const double denominator = 1.0 - p;
        if (denominator <= 1e-12)
            return distribution(all, task);
        const int vmax = m.getVmax();
        std::vector<double> without(vmax + 1);
        without[0] = d[0] / denominator;
        for (int v = 1; v <= vmax; ++v)
            without[v] = (d[v] - p * without[v - 1]) / denominator;
        return without;
    }

    bool moveRespectsCapacityAndSmax(const Allocation& all, int task, int from, int to) const {
        if (all.load[to] + m.getTaskConsumption(task) > m.getVres())
            return false;
        int employed = all.employed;
        if (all.count[from] == 1)
            --employed;
        if (all.count[to] == 0)
            ++employed;
        return employed <= m.getSmax();
    }

public:
    Ils(const InstanceMatrix& matrix, bool keepBestOption, bool incrementalSla = false,
        bool serviceReplacement = false)
        : m(matrix), keepBest(keepBestOption), incremental(incrementalSla),
          replacement(serviceReplacement), n(matrix.getNumberOfTasks()),
          services(matrix.getNumberOfServices()) {}

    // ProbabilityBasedGreedyInitialSolution.
    Allocation initialSolution() {
        Allocation all(n, services);
        while (all.allocated < n) {
            for (int task = 0; task < n; ++task) {
                std::vector<int> ordered(services);
                for (int i = 0; i < services; ++i)
                    ordered[i] = i;
                // O mesmo bubble sort do Java (estável, crescente pela probabilidade).
                for (int i = 0; i < services; ++i)
                    for (int j = 0; j < services - 1; ++j)
                        if (m.getServiceProb(ordered[j]) > m.getServiceProb(ordered[j + 1]))
                            std::swap(ordered[j], ordered[j + 1]);
                do {
                    if (ordered.empty())
                        throw std::runtime_error("construção inicial sem serviço viável");
                    all.addTask(task, ordered.front(), m);
                    if (!validator.isFeasible(all, m, true)) {
                        all.removeTask(task, m);
                        ordered.erase(ordered.begin());
                    }
                } while (all.service[task] < 0);
            }
        }
        return all;
    }

    // ImprovedSearcher.bestImprovement: melhor melhoria na vizinhança MOVE com as mesmas
    // regras do original e, opcionalmente, avaliação incremental do SLA e substituição
    // de serviço (troca de um serviço empregado por um vazio, com todas as tarefas).
    void improvedSearch(Allocation& all) {
        const int vmax = m.getVmax();
        const double pmax = m.getPmax();
        int movesSinceRebuild = 0;
        std::vector<double> dist;
        double violation = 0.0;
        if (incremental) {
            dist = distribution(all);
            violation = violationProbability(dist);
        }
        while (true) {
            // Distribuição sem uma tarefa do serviço a: depende só de p_a.
            std::vector<std::vector<double>> without(incremental ? services : 0);
            int bestTask = -1, bestService = -1, bestGain = 0;
            for (int task = 0; task < n; ++task) {
                const int cur = all.service[task];
                const int curCost = m.getTaskCost(task, cur);
                const int init = javaRandomIndex(services);
                for (int k = 0; k < services; ++k) {
                    const int s = (init + k) % services;
                    if (s == cur)
                        continue;
                    const int gain = curCost - m.getTaskCost(task, s);
                    if (gain <= 0 || gain <= bestGain)
                        continue;
                    const bool slaCheck = m.getServiceProb(s) > m.getServiceProb(cur);
                    bool feasible;
                    if (incremental) {
                        feasible = moveRespectsCapacityAndSmax(all, task, cur, s);
                        if (feasible && slaCheck) {
                            if (without[cur].empty())
                                without[cur] = removeOne(dist, m.getServiceProb(cur), all, task);
                            const double newViolation = violation
                                + (m.getServiceProb(s) - m.getServiceProb(cur)) * without[cur][vmax];
                            feasible = newViolation <= pmax + kSlaTolerance;
                        }
                    } else {
                        all.replaceService(task, s, m);
                        feasible = validator.isFeasible(all, m, slaCheck);
                        all.replaceService(task, cur, m);
                    }
                    if (feasible) {
                        bestGain = gain;
                        bestTask = task;
                        bestService = s;
                    }
                }
            }

            int bestSource = -1, bestTarget = -1;
            if (replacement) {
                std::vector<std::vector<int>> tasksOf(services);
                for (int t = 0; t < n; ++t)
                    tasksOf[all.service[t]].push_back(t);
                for (int a = 0; a < services; ++a) {
                    if (tasksOf[a].empty())
                        continue;
                    for (int b = 0; b < services; ++b) {
                        if (b == a || !tasksOf[b].empty())
                            continue;
                        int gain = 0;
                        for (int t : tasksOf[a])
                            gain += m.getTaskCost(t, a) - m.getTaskCost(t, b);
                        if (gain <= 0 || gain <= bestGain)
                            continue;
                        // Mesma capacidade em todos os serviços e mesmo número de
                        // serviços empregados: só o SLA pode ser violado, se p_b > p_a.
                        bool feasible = true;
                        if (m.getServiceProb(b) > m.getServiceProb(a)) {
                            for (int t : tasksOf[a])
                                all.replaceService(t, b, m);
                            feasible = validator.isFeasible(all, m, true);
                            for (int t : tasksOf[a])
                                all.replaceService(t, a, m);
                        }
                        if (feasible) {
                            bestGain = gain;
                            bestSource = a;
                            bestTarget = b;
                        }
                    }
                }
            }

            if (bestGain <= 0)
                break;
            if (bestSource >= 0) {
                for (int t = 0; t < n; ++t)
                    if (all.service[t] == bestSource)
                        all.replaceService(t, bestTarget, m);
                if (incremental) {
                    dist = distribution(all);
                    violation = violationProbability(dist);
                    movesSinceRebuild = 0;
                }
            } else {
                const int old = all.service[bestTask];
                if (incremental) {
                    std::vector<double> w = without[old].empty()
                        ? removeOne(dist, m.getServiceProb(old), all, bestTask) : without[old];
                    const double p = m.getServiceProb(bestService);
                    for (int v = 0; v <= vmax; ++v)
                        dist[v] = w[v] * (1.0 - p) + (v > 0 ? w[v - 1] * p : 0.0);
                    violation = violationProbability(dist);
                }
                all.replaceService(bestTask, bestService, m);
                if (incremental && ++movesSinceRebuild >= kRebuildInterval) {
                    dist = distribution(all);
                    violation = violationProbability(dist);
                    movesSinceRebuild = 0;
                }
            }
        }
    }

    // GenericSearcher.costImprovement, BestImprovement, MOVE. Com I ou S ligados, a busca
    // é a do ImprovedSearcher (ILS.neighborhoodSearch do projeto Java).
    void localSearch(Allocation& all) {
        if (incremental || replacement) {
            improvedSearch(all);
            return;
        }
        bool improved;
        do {
            improved = false;
            int bestTask = -1, bestService = -1, bestGain = -1;
            for (int task = 0; task < n; ++task) {
                const int init = javaRandomIndex(services);
                const auto scan = [&](int from, int to) {
                    for (int s = from; s < to; ++s) {
                        const int cur = all.service[task];
                        if (s == cur || m.getTaskCost(task, s) >= m.getTaskCost(task, cur))
                            continue;
                        all.replaceService(task, s, m);
                        const bool verifyProb = m.getServiceProb(s) > m.getServiceProb(cur);
                        const bool feasible = validator.isFeasible(all, m, verifyProb);
                        all.replaceService(task, cur, m);
                        if (feasible) {
                            const int gain = m.getTaskCost(task, cur) - m.getTaskCost(task, s);
                            if (gain > bestGain) {
                                bestGain = gain;
                                bestTask = task;
                                bestService = s;
                            }
                        }
                    }
                };
                scan(init, services);
                scan(0, init);
            }
            if (bestGain != -1) {
                improved = true;
                all.replaceService(bestTask, bestService, m);
            }
        } while (improved);
    }

    // ILS.perturbationMove.
    void perturbation(Allocation& all, int iteration, int iterations) {
        const int moves = static_cast<int>(6 - (iteration / (iterations * 1.0)) * 5);
        for (int j = 0; j < moves; ++j) {
            const int task = javaRandomIndex(n);
            const int oldService = all.service[task];
            const int decided = javaRandomIndex(n);  // getRandomService(numberOfTasks)
            const int applied = keepBest ? decided : javaRandomIndex(n);
            all.replaceService(task, applied, m);
            const bool verifyProb = m.getServiceProb(decided) > m.getServiceProb(oldService);
            if (!validator.isFeasible(all, m, verifyProb)) {
                all.replaceService(task, oldService, m);
                --j;
            }
        }
    }

    // ILS.ILS: devolve a solução final da chamada (no original, o último ótimo local).
    Allocation run(int iterations, double startMs, double budgetSeconds) {
        Allocation best = initialSolution();
        localSearch(best);
        double bestCost = best.cost;
        best.timeToBest = nowMs() - startMs;
        // No original, a perturbação atua sobre o próprio objeto da melhor solução.
        for (int i = 0; i < iterations; ++i) {
            if ((nowMs() - startMs) / 1000.0 >= budgetSeconds)
                break;
            if (keepBest) {
                Allocation start = best;
                perturbation(start, i, iterations);
                localSearch(start);
                if (start.cost < bestCost) {
                    bestCost = start.cost;
                    best = start;
                    best.timeToBest = nowMs() - startMs;
                }
            } else {
                perturbation(best, i, iterations);
                localSearch(best);
                if (best.cost < bestCost) {
                    bestCost = best.cost;
                    best.timeToBest = nowMs() - startMs;
                }
            }
        }
        return best;
    }

    bool finalCheck(const Allocation& all) { return validator.isFeasible(all, m, true); }
};

int envInt(const char* name, int fallback) {
    const char* v = std::getenv(name);
    return v && *v ? std::stoi(v) : fallback;
}

double envDouble(const char* name, double fallback) {
    const char* v = std::getenv(name);
    return v && *v ? std::stod(v) : fallback;
}

std::vector<int> selectedInstances() {
    const char* raw = std::getenv("SSO_INSTANCES");
    if (!raw || !*raw)
        return listInstanceIds();
    std::vector<int> ids;
    std::istringstream input(raw);
    std::string token;
    while (std::getline(input, token, ','))
        ids.push_back(std::stoi(token));
    return ids;
}

}  // namespace

int main() {
    try {
        const int repetitions = envInt("SSO_REPETITIONS", 20);
        const double budget = envDouble("SSO_TIME_SECONDS", 10.0);
        const bool keepBest = envInt("SSO_ILS_KEEPBEST", 0) == 1;
        const bool incremental = envInt("SSO_ILS_INCREMENTAL", 0) == 1;
        const bool replacement = envInt("SSO_ILS_REPLACEMENT", 0) == 1;
        const int iterations = envInt("SSO_ILS_ITERATIONS", 10000);
        const bool singleCall = envInt("SSO_ILS_SINGLE_CALL", 0) == 1;
        const char* csvPath = std::getenv("SSO_CSV");
        std::ofstream csv;
        if (csvPath && *csvPath) {
            csv.open(csvPath);
            csv << "instance,repetition,optimum,cost,gap_pct,reached_optimal,time_to_best_ms,"
                   "elapsed_ms,budget_seconds,ils,incremental,replacement,keepbest,feasible\n";
        }
        std::cout << "CONFIG ils=3 cpp=1 incremental=" << incremental << " replacement="
                  << replacement << " keepBest=" << keepBest << " iterations=" << iterations
                  << " time=" << budget << " repetitions=" << repetitions
                  << " singleCall=" << singleCall << std::endl;

        for (int id : selectedInstances()) {
            InstanceMatrix instance = readInstance(id);
            if (!loadReferenceLog(instance, id))
                instance.setOptimalCost(trivialLowerBound(instance));
            const double optimum = instance.getOptimalCost();
            std::cout << "Executing instance " << id << std::endl;
            for (int r = 0; r < repetitions; ++r) {
                Ils ils(instance, keepBest, incremental, replacement);
                double bestCost = std::numeric_limits<double>::max();
                double timeToBest = -1.0;
                bool bestFeasible = false;
                const double start = nowMs();
                if (singleCall) {
                    Allocation all = ils.run(iterations, start,
                                             std::numeric_limits<double>::infinity());
                    bestCost = all.cost;
                    timeToBest = all.timeToBest;
                    bestFeasible = ils.finalCheck(all);
                } else {
                    do {
                        Allocation all = ils.run(iterations, start, budget);
                        if (all.cost < bestCost) {
                            bestCost = all.cost;
                            timeToBest = all.timeToBest;
                            bestFeasible = ils.finalCheck(all);
                        }
                        if (all.cost == optimum) {
                            timeToBest = all.timeToBest;
                            break;
                        }
                    } while ((nowMs() - start) / 1000.0 <= budget);
                }
                const double elapsed = nowMs() - start;
                if (!singleCall && bestCost != optimum)
                    timeToBest = elapsed;
                std::cout << "r " << r << " cost " << bestCost << std::endl;
                if (csv.is_open())
                    csv << id << ',' << r + 1 << ',' << static_cast<int>(optimum) << ','
                        << bestCost << ',' << 100.0 * (bestCost - optimum) / optimum << ','
                        << (bestCost == optimum ? 1 : 0) << ',' << timeToBest << ','
                        << elapsed << ',' << budget << ",3," << (incremental ? 1 : 0) << ','
                        << (replacement ? 1 : 0) << ',' << (keepBest ? 1 : 0) << ','
                        << (bestFeasible ? 1 : 0) << std::endl;
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "[ERRO] " << error.what() << "\n";
        return 1;
    }
    return 0;
}
