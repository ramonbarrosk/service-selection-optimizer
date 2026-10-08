// Estratégias para escapar do mínimo local 104 da instância 100 com a GLS
// promovida. Uma execução por processo; o script run_escape_instance100.sh
// dispara a grade em paralelo.
//
// SSO_STRATEGY:
//   base     GLS promovida (alpha 0.3, penalidades persistentes)
//   alpha    GLS promovida com SSO_ALPHA diferente
//   oracle   diagnóstico: só os 39 serviços da solução ótima ficam baratos
//   restart  GLS + reinício: após SSO_STAGNATION_ROUNDS rodadas sem melhora,
//            zera as penalidades e perturba a melhor solução trocando
//            SSO_KICK serviços empregados por serviços vazios aleatórios
//   osc      GLS + oscilação estratégica (capacidade relaxada) na estagnação
// Outras variáveis: SSO_INSTANCE (100), SSO_TIME_SECONDS (60), SSO_SEED (1).
// Saída: trajetória de melhorias e uma linha final "RESULT,..." para CSV.

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "Allocation.h"
#include "GenericSearcher.h"
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

std::string env(const char* name, const std::string& fallback) {
    const char* raw = std::getenv(name);
    return raw && *raw ? raw : fallback;
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

// Troca `kick` serviços empregados por serviços vazios, transferindo todas as
// tarefas. Capacidade e Smax são preservados; a SLA é conferida a cada troca.
Allocation perturb(const Allocation& source, const InstanceMatrix& matrix, int kick) {
    Allocation result = source;
    for (int done = 0, attempts = 0; done < kick && attempts < 200; ++attempts) {
        std::vector<int> used, empty;
        for (int s = 0; s < matrix.getNumberOfServices(); ++s)
            (result.getTaskCountPerService()[s] ? used : empty).push_back(s);
        const int from = used[RandomUtil::getRandomNumber(used.size())];
        const int to = empty[RandomUtil::getRandomNumber(empty.size())];
        Allocation candidate = result;
        for (int t = 0; t < matrix.getNumberOfTasks(); ++t)
            if (candidate.getServiceForTask(t) == from)
                candidate.replaceService(Task(t, matrix.getTaskConsumption(t)),
                                         Service(to), matrix);
        if (feasible(candidate, matrix)) {
            result = candidate;
            ++done;
        }
    }
    return result;
}

}  // namespace

int main() {
    const int id = std::stoi(env("SSO_INSTANCE", "100"));
    const std::string strategy = env("SSO_STRATEGY", "base");
    const double seconds = std::stod(env("SSO_TIME_SECONDS", "60"));
    const unsigned seed = std::stoul(env("SSO_SEED", "1"));
    const double alpha = std::stod(env("SSO_ALPHA", "0.3"));
    const int kick = std::stoi(env("SSO_KICK", "3"));
    const long long stagnationRounds = std::stoll(env("SSO_STAGNATION_ROUNDS", "3000"));
    RandomUtil::setSeed(seed);

    InstanceMatrix matrix = readInstance(id);
    const auto [optimum, optimalAssignment] =
        readOptimum(id, matrix.getNumberOfTasks());
    if (strategy == "oracle") {
        std::vector<char> inOptimum(matrix.getNumberOfServices(), 0);
        for (int s : optimalAssignment)
            inOptimum[s] = 1;
        for (int t = 0; t < matrix.getNumberOfTasks(); ++t)
            for (int s = 0; s < matrix.getNumberOfServices(); ++s)
                if (!inOptimum[s])
                    matrix.setTaskCost(t, s, 1000);
    }

    const Allocation initial = probabilityInitialSolution(matrix);
    GuidedLocalSearcher gls;
    GenericSearcher generic;
    ProbabilityScenario scenario = ProbabilityScenario::Ps;
    Allocation current = initial;
    Allocation best = initial;
    const double start = nowMs();
    const double deadline = start + seconds * 1000.0;
    double timeToBest = 0.0;
    long long roundsAtBest = 0;
    long long roundsOffset = 0;  // rodadas acumuladas antes de cada reinício
    int restarts = 0;
    int oscillations = 0;
    bool resetPenalties = false;

    std::cout << std::fixed << std::setprecision(2);
    while (nowMs() < deadline && best.getCurrentCost() > optimum) {
        current = gls.improve(current, matrix, matrix.getVmax(), matrix.getSmax(),
                              matrix.getPmax(), scenario, alpha, 30,
                              !resetPenalties, deadline);
        if (resetPenalties) {
            resetPenalties = false;
        }
        const long long rounds = roundsOffset + gls.stats().completedPenaltyRounds;
        if (current.getCurrentCost() < best.getCurrentCost() - 1e-9) {
            best = current;
            timeToBest = nowMs() - start;
            roundsAtBest = rounds;
            std::cout << "  t=" << std::setw(8) << timeToBest / 1000.0
                      << " s  custo " << best.getCurrentCost() << '\n';
        }
        if (rounds - roundsAtBest < stagnationRounds)
            continue;

        if (strategy == "restart") {
            roundsOffset = rounds;
            roundsAtBest = rounds;
            current = perturb(best, matrix, kick);
            resetPenalties = true;
            ++restarts;
        } else if (strategy == "osc") {
            roundsAtBest = rounds;
            ++oscillations;
            Allocation probe = current;
            if (generic.oscillationImprovement(probe, matrix, matrix.getVmax(),
                                               matrix.getSmax(), matrix.getPmax(),
                                               scenario, 12, deadline)
                    && feasible(probe, matrix)) {
                current = probe;
                if (current.getCurrentCost() < best.getCurrentCost() - 1e-9) {
                    best = current;
                    timeToBest = nowMs() - start;
                    std::cout << "  t=" << std::setw(8) << timeToBest / 1000.0
                              << " s  custo " << best.getCurrentCost() << " (osc)\n";
                }
            }
        }
    }

    std::cout << "RESULT," << id << ',' << strategy << ',' << seed << ','
              << alpha << ',' << kick << ',' << stagnationRounds << ','
              << seconds << ',' << best.getCurrentCost() << ',' << optimum << ','
              << feasible(best, matrix) << ',' << timeToBest / 1000.0 << ','
              << (nowMs() - start) / 1000.0 << ',' << restarts << ','
              << oscillations << '\n';
    return 0;
}
