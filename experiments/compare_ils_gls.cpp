#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
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

namespace fs = std::filesystem;

namespace {

constexpr double kGlsAlpha = 0.3;
constexpr int kDefaultGlsRoundsPerCycle = 30;

double nowMs() {
    using namespace std::chrono;
    return static_cast<double>(duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count());
}

int envInt(const char* name, int fallback) {
    const char* value = std::getenv(name);
    if (!value || !*value)
        return fallback;
    const int parsed = std::stoi(value);
    if (parsed <= 0)
        throw std::invalid_argument(std::string(name) + " deve ser maior que zero");
    return parsed;
}

double optionalEnvDouble(const char* name) {
    const char* value = std::getenv(name);
    if (!value || !*value)
        return -1.0;
    const double parsed = std::stod(value);
    if (parsed <= 0.0)
        throw std::invalid_argument(std::string(name) + " deve ser maior que zero");
    return parsed;
}

std::string envString(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return value && *value ? value : fallback;
}

std::vector<int> selectedInstances() {
    const char* raw = std::getenv("SSO_INSTANCES");
    if (!raw || !*raw) {
        std::vector<int> all;
        for (const auto& entry : fs::directory_iterator("data/instances")) {
            const std::string filename = entry.path().filename().string();
            const auto separator = filename.rfind('_');
            if (separator != std::string::npos)
                all.push_back(std::stoi(filename.substr(separator + 1)));
        }
        std::sort(all.begin(), all.end());
        return all;
    }

    std::vector<int> ids;
    std::istringstream input(raw);
    std::string token;
    while (std::getline(input, token, ',')) {
        const int id = std::stoi(token);
        const fs::path path = "data/instances/Instance_10_10_" + std::to_string(id);
        if (!fs::exists(path))
            throw std::invalid_argument("Instância inexistente em SSO_INSTANCES: "
                                        + std::to_string(id));
        ids.push_back(id);
    }
    if (ids.empty())
        throw std::invalid_argument("SSO_INSTANCES não pode ser vazio");
    return ids;
}

InstanceMatrix readInstance(int id) {
    const std::string path = "data/instances/Instance_10_10_" + std::to_string(id);
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Não foi possível abrir: " + path);

    bool firstLine = true;
    InstanceMatrix instance;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty())
            continue;

        std::istringstream input(line);
        std::vector<std::string> tokens;
        std::string token;
        while (input >> token)
            tokens.push_back(token);
        if (tokens.empty())
            continue;

        if (firstLine) {
            instance = InstanceMatrix(
                std::stoi(tokens[2]), std::stoi(tokens[1]), std::stoi(tokens[3]),
                std::stoi(tokens[5]), std::stod(tokens[4]), std::stoi(tokens[6]));
            firstLine = false;
        } else if (tokens[0] == "r") {
            instance.setResourceConsumption(std::stoi(tokens[1]) - 1,
                                            std::stoi(tokens[2]));
        } else if (tokens[0] == "p") {
            instance.setSlaViolationProbability(std::stoi(tokens[1]) - 1,
                                                std::stod(tokens[2]));
        } else if (tokens[0] == "c") {
            instance.setTaskCost(std::stoi(tokens[2]) - 1,
                                 std::stoi(tokens[1]) - 1,
                                 std::stoi(tokens[3]));
        }
    }

    for (int service = 0; service < instance.getNumberOfServices(); ++service)
        instance.setServResourceCapacity(service, instance.getVres());
    instance.setInstanceName("Instance_10_10_" + std::to_string(id));
    return instance;
}

void loadReferenceLog(InstanceMatrix& instance, int id) {
    std::ifstream file("data/Log/Instance_10_10_" + std::to_string(id));
    if (!file.is_open())
        return;

    std::string line;
    std::string lastLine;
    while (std::getline(file, line)) {
        if (line.rfind("Total", 0) == 0) {
            const auto equal = line.find('=');
            if (equal != std::string::npos) {
                std::string value = line.substr(equal + 1);
                const auto seconds = value.find("sec");
                if (seconds != std::string::npos)
                    value = value.substr(0, seconds);
                instance.setOptimalExecTime(std::stod(value));
            }
        }
        lastLine = line;
    }

    const auto equal = lastLine.rfind("= ");
    if (equal != std::string::npos)
        instance.setOptimalCost(std::stoi(lastLine.substr(equal + 2)));
}

// Lê o tempo médio até a melhor solução registrado na execução Java original.
// Formato da linha: nome ótimo tempo-exato média melhor tempo-até-melhor.
std::map<int, double> loadJavaTimeToBest() {
    const std::string path = "data/report_java.txt";
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Não foi possível abrir: " + path);

    std::map<int, double> times;
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("Instance_10_10_", 0) != 0)
            continue;

        std::istringstream input(line);
        std::string name;
        double optimum = 0.0;
        double exactTime = 0.0;
        double meanCost = 0.0;
        double bestCost = 0.0;
        double timeToBest = 0.0;
        if (!(input >> name >> optimum >> exactTime >> meanCost
                    >> bestCost >> timeToBest))
            throw std::runtime_error("Linha Java inválida: " + line);

        const auto separator = name.rfind('_');
        if (separator == std::string::npos || timeToBest <= 0.0)
            throw std::runtime_error("Tempo Java inválido: " + line);
        times[std::stoi(name.substr(separator + 1))] = timeToBest;
    }
    if (times.empty())
        throw std::runtime_error("Nenhum tempo Java encontrado em " + path);
    return times;
}

double javaMeasuredBudgetSeconds(int instanceId,
                                 const std::map<int, double>& javaTimes,
                                 double minimumBudgetSeconds) {
    const auto found = javaTimes.find(instanceId);
    if (found == javaTimes.end())
        throw std::runtime_error(
            "Tempo da versão Java ausente para a instância "
            + std::to_string(instanceId));
    return std::max(minimumBudgetSeconds, found->second);
}

Allocation probabilityInitialSolution(const InstanceMatrix& matrix) {
    SolutionValidator validator;
    ProbabilityScenario scenario = ProbabilityScenario::Ps;
    Allocation allocation;

    std::vector<int> services(matrix.getNumberOfServices());
    std::iota(services.begin(), services.end(), 0);
    std::sort(services.begin(), services.end(), [&](int first, int second) {
        return matrix.getServiceProb(first) < matrix.getServiceProb(second);
    });

    for (int taskId = 0; taskId < matrix.getNumberOfTasks(); ++taskId) {
        Task task(taskId, matrix.getTaskConsumption(taskId));
        bool assigned = false;
        for (int serviceId : services) {
            allocation.addTask(task, Service(serviceId), matrix);
            if (validator.isFeasible(
                    matrix, allocation, matrix.getVmax(), matrix.getSmax(),
                    matrix.getPmax(), scenario, true)) {
                assigned = true;
                break;
            }
            allocation.removeTask(task, matrix);
        }
        if (!assigned)
            throw std::runtime_error("O construtivo por probabilidade não encontrou solução viável");
    }
    return allocation;
}

// Construtivo guloso aleatório do ILS original. A mesma solução produzida aqui
// é copiada para os três métodos, isolando o efeito da estratégia de busca.
Allocation commonInitialSolution(const InstanceMatrix& matrix, double alpha) {
    SolutionValidator validator;
    ProbabilityScenario scenario = ProbabilityScenario::Ps;
    Allocation allocation;
    std::vector<int> pending(matrix.getNumberOfTasks());
    std::iota(pending.begin(), pending.end(), 0);
    int failedAttempts = 0;

    while (!pending.empty()) {
        if (failedAttempts > 3 * matrix.getNumberOfTasks())
            return probabilityInitialSolution(matrix);

        const int position = RandomUtil::getRandomInt(0, static_cast<int>(pending.size()) - 1);
        const int taskId = pending[position];
        const double minimum = matrix.getMinCostForTask(taskId);
        const double maximum = matrix.getMaxCostForTask(taskId);
        const auto candidates = matrix.getServicesWithMaxCost(
            taskId, minimum + alpha * (maximum - minimum));

        Task task(taskId, matrix.getTaskConsumption(taskId));
        Service service = RandomUtil::getRandomService(candidates);
        pending.erase(pending.begin() + position);
        allocation.addTask(task, service, matrix);

        if (!validator.isFeasible(
                matrix, allocation, matrix.getVmax(), matrix.getSmax(),
                matrix.getPmax(), scenario, true)) {
            allocation.removeTask(task, matrix);
            pending.push_back(taskId);
            ++failedAttempts;
        }
    }
    return allocation;
}

Allocation perturbMove(Allocation allocation, const InstanceMatrix& matrix,
                       int iteration, int maximumIterations, double deadlineMs) {
    SolutionValidator validator;
    ProbabilityScenario scenario = ProbabilityScenario::Ps;
    const int intensity = static_cast<int>(
        6 - (iteration / static_cast<double>(maximumIterations)) * 5);

    int accepted = 0;
    while (accepted < intensity && nowMs() < deadlineMs) {
        const int taskId = RandomUtil::getRandomInt(0, matrix.getNumberOfTasks() - 1);
        const int oldService = allocation.getServiceForTask(taskId);
        const int newService = RandomUtil::getRandomInt(0, matrix.getNumberOfServices() - 1);
        Task task(taskId, matrix.getTaskConsumption(taskId));
        allocation.replaceService(task, Service(newService), matrix);

        const bool checkProbability =
            matrix.getServiceProb(newService) > matrix.getServiceProb(oldService);
        if (!validator.isFeasible(
                matrix, allocation, matrix.getVmax(), matrix.getSmax(),
                matrix.getPmax(), scenario, checkProbability)) {
            allocation.replaceService(task, Service(oldService), matrix);
        } else {
            ++accepted;
        }
    }
    return allocation;
}

struct RunResult {
    double cost = std::numeric_limits<double>::infinity();
    double elapsedMs = 0.0;
    bool feasible = false;
};

bool isFeasible(const Allocation& allocation, const InstanceMatrix& matrix) {
    SolutionValidator validator;
    ProbabilityScenario scenario = ProbabilityScenario::Ps;
    Allocation copy = allocation;
    return validator.isFeasible(
        matrix, copy, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(),
        scenario, true);
}

RunResult runOriginalIls(const Allocation& initial, const InstanceMatrix& matrix,
                         int maximumIterations, double budgetSeconds) {
    const double start = nowMs();
    const double deadline = start + budgetSeconds * 1000.0;
    Allocation globalBest = initial;
    GenericSearcher searcher;
    ProbabilityScenario scenario = ProbabilityScenario::Ps;

    while (nowMs() < deadline) {
        Allocation current = initial;
        searcher.costImprovement(
            current, matrix, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(),
            scenario, ImprovementCondition::FIRST_IMPROVEMENT, ImprovementMode::SWAP);
        if (current.getCurrentCost() < globalBest.getCurrentCost())
            globalBest = current;

        for (int iteration = 0;
             iteration < maximumIterations && nowMs() < deadline;
             ++iteration) {
            Allocation candidate = perturbMove(
                current, matrix, iteration, maximumIterations, deadline);
            if (nowMs() >= deadline)
                break;
            searcher.costImprovement(
                candidate, matrix, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(),
                scenario, ImprovementCondition::FIRST_IMPROVEMENT, ImprovementMode::SWAP);

            // O candidato vira a solução corrente para que o ILS visite novas bacias.
            current = candidate;
            if (current.getCurrentCost() < globalBest.getCurrentCost())
                globalBest = current;
            if (matrix.getOptimalCost() > 0
                    && globalBest.getCurrentCost() <= matrix.getOptimalCost())
                break;
        }

        if (matrix.getOptimalCost() > 0
                && globalBest.getCurrentCost() <= matrix.getOptimalCost())
            break;
    }

    return {globalBest.getCurrentCost(), nowMs() - start,
            isFeasible(globalBest, matrix)};
}

RunResult runStandaloneGls(const Allocation& initial, const InstanceMatrix& matrix,
                           double budgetSeconds, int roundsPerCycle,
                           bool withOscillation) {
    const double start = nowMs();
    const double deadline = start + budgetSeconds * 1000.0;
    Allocation current = initial;
    Allocation globalBest = initial;
    GuidedLocalSearcher guidedSearcher;
    GenericSearcher searcher;
    ProbabilityScenario scenario = ProbabilityScenario::Ps;

    while (nowMs() < deadline) {
        current = guidedSearcher.improve(
            current, matrix, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(),
            scenario, kGlsAlpha, roundsPerCycle, true, deadline);
        if (current.getCurrentCost() < globalBest.getCurrentCost())
            globalBest = current;

        if (withOscillation && nowMs() < deadline) {
            searcher.oscillationImprovement(
                current, matrix, matrix.getVmax(), matrix.getSmax(),
                matrix.getPmax(), scenario);
            if (current.getCurrentCost() < globalBest.getCurrentCost())
                globalBest = current;
        }

        if (matrix.getOptimalCost() > 0
                && globalBest.getCurrentCost() <= matrix.getOptimalCost())
            break;
    }

    return {globalBest.getCurrentCost(), nowMs() - start,
            isFeasible(globalBest, matrix)};
}

double gapPercent(double cost, int optimum) {
    return optimum > 0 ? 100.0 * (cost - optimum) / optimum
                       : std::numeric_limits<double>::quiet_NaN();
}

struct Row {
    int instance = 0;
    int repetition = 0;
    unsigned seed = 0;
    int optimum = 0;
    double budgetSeconds = 0.0;
    double initialCost = 0.0;
    RunResult ils;
    RunResult gls;
    RunResult glsOscillation;
};

void writeCsv(const std::string& outputPath, const std::vector<Row>& rows) {
    const fs::path path(outputPath);
    if (path.has_parent_path())
        fs::create_directories(path.parent_path());
    std::ofstream output(path);
    if (!output.is_open())
        throw std::runtime_error("Não foi possível criar: " + outputPath);

    output << "instance,repetition,seed,optimum,budget_seconds,initial_cost,"
              "ils_cost,ils_gap_pct,ils_time_ms,ils_feasible,"
              "gls_cost,gls_gap_pct,gls_time_ms,gls_feasible,"
              "gls_osc_cost,gls_osc_gap_pct,gls_osc_time_ms,gls_osc_feasible\n";
    output << std::fixed << std::setprecision(6);
    for (const Row& row : rows) {
        output << row.instance << ',' << row.repetition << ',' << row.seed << ','
               << row.optimum << ',' << row.budgetSeconds << ',' << row.initialCost << ','
               << row.ils.cost << ',' << gapPercent(row.ils.cost, row.optimum) << ','
               << row.ils.elapsedMs << ',' << row.ils.feasible << ','
               << row.gls.cost << ',' << gapPercent(row.gls.cost, row.optimum) << ','
               << row.gls.elapsedMs << ',' << row.gls.feasible << ','
               << row.glsOscillation.cost << ','
               << gapPercent(row.glsOscillation.cost, row.optimum) << ','
               << row.glsOscillation.elapsedMs << ','
               << row.glsOscillation.feasible << '\n';
    }
}

void printSummary(const std::vector<Row>& rows) {
    struct Summary {
        double cost = 0.0;
        double gap = 0.0;
        double time = 0.0;
        int optimums = 0;
        int feasible = 0;
    };
    Summary summaries[3];

    for (const Row& row : rows) {
        const RunResult results[] = {row.ils, row.gls, row.glsOscillation};
        for (int method = 0; method < 3; ++method) {
            summaries[method].cost += results[method].cost;
            summaries[method].gap += gapPercent(results[method].cost, row.optimum);
            summaries[method].time += results[method].elapsedMs;
            summaries[method].feasible += results[method].feasible ? 1 : 0;
            if (row.optimum > 0 && results[method].cost <= row.optimum)
                ++summaries[method].optimums;
        }
    }

    const char* names[] = {"ILS original", "GLS sem ILS", "GLS + oscilação sem ILS"};
    std::cout << "\nRESUMO POR EXECUÇÃO\n";
    std::cout << std::left << std::setw(27) << "Método"
              << std::right << std::setw(14) << "Custo médio"
              << std::setw(13) << "GAP médio"
              << std::setw(14) << "Tempo médio"
              << std::setw(12) << "Ótimos" << '\n';
    for (int method = 0; method < 3; ++method) {
        const double count = static_cast<double>(rows.size());
        std::cout << std::left << std::setw(27) << names[method]
                  << std::right << std::setw(14) << std::fixed << std::setprecision(2)
                  << summaries[method].cost / count
                  << std::setw(12) << summaries[method].gap / count << "%"
                  << std::setw(12) << summaries[method].time / count << " ms"
                  << std::setw(8) << summaries[method].optimums << '/' << rows.size()
                  << '\n';
    }
}

}  // namespace

int main() {
    const int repetitions = envInt("SSO_REPETITIONS", 3);
    const int maximumIterations = envInt("SSO_ITERATIONS", 10000);
    const int roundsPerCycle = envInt("SSO_GLS_ROUNDS", kDefaultGlsRoundsPerCycle);
    const int baseSeed = envInt("SSO_SEED", 20260818);
    // SSO_TIME_SECONDS ainda permite um limite fixo. Sem ele, cada instância
    // usa o tempo ao melhor medido no Java, nunca menos que o piso configurado.
    const double fixedBudgetSeconds = optionalEnvDouble("SSO_TIME_SECONDS");
    const double configuredMinimum = optionalEnvDouble("SSO_MIN_TIME_SECONDS");
    const double minimumBudgetSeconds = configuredMinimum > 0.0
        ? configuredMinimum : 0.55;
    const std::map<int, double> javaTimes = fixedBudgetSeconds > 0.0
        ? std::map<int, double>{} : loadJavaTimeToBest();
    const std::string outputPath = envString(
        "SSO_EXPERIMENT_OUTPUT", "data/experiments/ils_gls_comparison.csv");
    const std::vector<int> instances = selectedInstances();

    std::cout << "Experimento: ILS original x GLS sem ILS x GLS+oscilação sem ILS\n"
              << "Instâncias: " << instances.size()
              << " | repetições: " << repetitions
              << " | orçamento por método/repetição: "
              << (fixedBudgetSeconds > 0.0
                    ? std::to_string(fixedBudgetSeconds) + " s (fixo)"
                    : "max(" + std::to_string(minimumBudgetSeconds)
                        + " s, tempo ao melhor da versão Java)") << "\n"
              << "Solução inicial pareada | alpha GLS: " << kGlsAlpha
              << " | rodadas GLS por ciclo: " << roundsPerCycle << "\n";

    std::vector<Row> rows;
    rows.reserve(instances.size() * repetitions);
    for (int instanceId : instances) {
        InstanceMatrix matrix = readInstance(instanceId);
        loadReferenceLog(matrix, instanceId);
        const double budgetSeconds = fixedBudgetSeconds > 0.0
            ? fixedBudgetSeconds
            : javaMeasuredBudgetSeconds(
                instanceId, javaTimes, minimumBudgetSeconds);

        std::cout << "Instância " << std::setw(3) << instanceId
                  << " | orçamento: " << std::fixed << std::setprecision(3)
                  << budgetSeconds << " s por método/repetição\n";

        for (int repetition = 0; repetition < repetitions; ++repetition) {
            const unsigned seed = static_cast<unsigned>(
                baseSeed + instanceId * 1009 + repetition * 9176);
            RandomUtil::setSeed(seed);
            const Allocation initial = commonInitialSolution(matrix, 0.4);

            // A semente é restaurada antes do ILS para separar a aleatoriedade
            // da construção inicial daquela usada nas perturbações.
            RandomUtil::setSeed(seed + 1);
            Row row;
            row.instance = instanceId;
            row.repetition = repetition + 1;
            row.seed = seed;
            row.optimum = matrix.getOptimalCost();
            row.budgetSeconds = budgetSeconds;
            row.initialCost = initial.getCurrentCost();
            row.ils = runOriginalIls(initial, matrix, maximumIterations, budgetSeconds);
            row.gls = runStandaloneGls(
                initial, matrix, budgetSeconds, roundsPerCycle, false);
            row.glsOscillation = runStandaloneGls(
                initial, matrix, budgetSeconds, roundsPerCycle, true);
            rows.push_back(row);

            std::cout << "Instância " << std::setw(3) << instanceId
                      << " | repetição " << repetition + 1
                      << " | inicial " << initial.getCurrentCost()
                      << " | ILS " << row.ils.cost
                      << " | GLS " << row.gls.cost
                      << " | GLS+OSC " << row.glsOscillation.cost << '\n';
        }
    }

    writeCsv(outputPath, rows);
    printSummary(rows);
    std::cout << "\nResultados detalhados: " << outputPath << '\n';
    return 0;
}
