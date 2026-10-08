#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Allocation.h"
#include "InstanceMatrix.hpp"
#include "ProbabilityScenario.h"
#include "RandomUtil.hpp"
#include "Service.h"
#include "SoftSlaGuidedLocalSearcher.hpp"
#include "SolutionValidator.hpp"
#include "Task.h"

namespace fs = std::filesystem;

namespace {

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

double envDouble(const char* name, double fallback) {
    const char* value = std::getenv(name);
    if (!value || !*value)
        return fallback;
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
        std::vector<int> ids;
        for (const auto& entry : fs::directory_iterator("data/instances")) {
            const std::string filename = entry.path().filename().string();
            const auto separator = filename.rfind('_');
            if (separator != std::string::npos)
                ids.push_back(std::stoi(filename.substr(separator + 1)));
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    std::vector<int> ids;
    std::istringstream input(raw);
    std::string token;
    while (std::getline(input, token, ',')) {
        const int id = std::stoi(token);
        const fs::path path = "data/instances/Instance_10_10_" + std::to_string(id);
        if (!fs::exists(path))
            throw std::invalid_argument("Instância inexistente: " + std::to_string(id));
        ids.push_back(id);
    }
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

void loadOptimalCost(InstanceMatrix& instance, int id) {
    std::ifstream file("data/Log/Instance_10_10_" + std::to_string(id));
    std::string line;
    std::string lastLine;
    while (std::getline(file, line))
        lastLine = line;
    const auto equal = lastLine.rfind("= ");
    if (equal != std::string::npos)
        instance.setOptimalCost(std::stoi(lastLine.substr(equal + 2)));
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
            throw std::runtime_error("Não foi possível criar a solução inicial viável");
    }
    return allocation;
}

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

bool isFeasible(const Allocation& allocation, const InstanceMatrix& matrix) {
    SolutionValidator validator;
    ProbabilityScenario scenario = ProbabilityScenario::Ps;
    Allocation copy = allocation;
    return validator.isFeasible(
        matrix, copy, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(),
        scenario, true);
}

struct Result {
    double cost = 0.0;
    double elapsedMs = 0.0;
    bool feasible = false;
    SoftSlaSearchStats softStats;
};

Result runOptimizedGls(const Allocation& initial, const InstanceMatrix& matrix,
                       double budgetSeconds, double probabilityBeta,
                       bool allowSoftSla, double softTailFraction,
                       double proximityFraction) {
    const double start = nowMs();
    const double deadline = start + budgetSeconds * 1000.0;
    const double softStart = start
        + budgetSeconds * 1000.0 * (1.0 - softTailFraction);
    ProbabilityScenario scenario = ProbabilityScenario::Ps;
    SoftSlaGuidedLocalSearcher searcher;
    Allocation best = searcher.improve(
        initial, matrix, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(),
        scenario, 0.3, probabilityBeta, INT_MAX, deadline,
        allowSoftSla, softStart, proximityFraction);
    return {best.getCurrentCost(), nowMs() - start,
            isFeasible(best, matrix), searcher.stats()};
}

Result runStrictGls(const Allocation& initial, const InstanceMatrix& matrix,
                    double budgetSeconds, double probabilityBeta) {
    return runOptimizedGls(
        initial, matrix, budgetSeconds, probabilityBeta,
        false, 0.0, 0.0);
}

Result runSoftSlaGls(const Allocation& initial, const InstanceMatrix& matrix,
                     double budgetSeconds, double probabilityBeta) {
    return runOptimizedGls(
        initial, matrix, budgetSeconds, probabilityBeta,
        true, 1.0, 1.0);
}

Result runConditionalGls(const Allocation& initial,
                         const InstanceMatrix& matrix,
                         double budgetSeconds, double probabilityBeta,
                         double softTailFraction,
                         double proximityFraction) {
    return runOptimizedGls(
        initial, matrix, budgetSeconds, probabilityBeta,
        true, softTailFraction, proximityFraction);
}

double gap(double cost, int optimum) {
    return optimum > 0 ? 100.0 * (cost - optimum) / optimum : 0.0;
}

struct Row {
    int instance = 0;
    int repetition = 0;
    unsigned seed = 0;
    int optimum = 0;
    double initialCost = 0.0;
    Result strict;
    Result soft;
    Result conditional;
};

void writeCsv(const std::string& path, const std::vector<Row>& rows,
              double budgetSeconds, double probabilityBeta,
              double softTailFraction, double proximityFraction) {
    const fs::path outputPath(path);
    if (outputPath.has_parent_path())
        fs::create_directories(outputPath.parent_path());
    std::ofstream output(outputPath);
    output << "instance,repetition,seed,optimum,budget_seconds,probability_beta,"
              "soft_tail_fraction,proximity_fraction,initial_cost,"
              "strict_cost,strict_gap_pct,strict_time_ms,strict_feasible,"
              "strict_candidates,strict_rounds,"
              "soft_cost,soft_gap_pct,soft_time_ms,soft_feasible,"
              "soft_candidates,soft_rounds,"
              "soft_evaluated_infeasible,soft_accepted_infeasible,"
              "soft_recoveries,soft_max_accepted_excess,soft_probability_drift,"
              "conditional_cost,conditional_gap_pct,conditional_time_ms,"
              "conditional_feasible,conditional_candidates,conditional_rounds,"
              "conditional_evaluated_infeasible,"
              "conditional_accepted_infeasible,conditional_recoveries,"
              "conditional_max_accepted_excess,conditional_probability_drift\n";
    output << std::fixed << std::setprecision(9);
    for (const Row& row : rows) {
        output << row.instance << ',' << row.repetition << ',' << row.seed << ','
               << row.optimum << ',' << budgetSeconds << ',' << probabilityBeta << ','
               << softTailFraction << ',' << proximityFraction << ','
               << row.initialCost << ','
               << row.strict.cost << ',' << gap(row.strict.cost, row.optimum) << ','
               << row.strict.elapsedMs << ',' << row.strict.feasible << ','
               << row.strict.softStats.evaluatedCandidates << ','
               << row.strict.softStats.completedPenaltyRounds << ','
               << row.soft.cost << ',' << gap(row.soft.cost, row.optimum) << ','
               << row.soft.elapsedMs << ',' << row.soft.feasible << ','
               << row.soft.softStats.evaluatedCandidates << ','
               << row.soft.softStats.completedPenaltyRounds << ','
               << row.soft.softStats.evaluatedInfeasibleCandidates << ','
               << row.soft.softStats.acceptedInfeasibleMoves << ','
               << row.soft.softStats.feasibleRecoveries << ','
               << row.soft.softStats.maximumAcceptedExcess << ','
               << row.soft.softStats.finalProbabilityDrift << ','
               << row.conditional.cost << ','
               << gap(row.conditional.cost, row.optimum) << ','
               << row.conditional.elapsedMs << ',' << row.conditional.feasible << ','
               << row.conditional.softStats.evaluatedCandidates << ','
               << row.conditional.softStats.completedPenaltyRounds << ','
               << row.conditional.softStats.evaluatedInfeasibleCandidates << ','
               << row.conditional.softStats.acceptedInfeasibleMoves << ','
               << row.conditional.softStats.feasibleRecoveries << ','
               << row.conditional.softStats.maximumAcceptedExcess << ','
               << row.conditional.softStats.finalProbabilityDrift << '\n';
    }
}

void printSummary(const std::vector<Row>& rows) {
    double strictGap = 0.0;
    double softGap = 0.0;
    double conditionalGap = 0.0;
    long softAccepted = 0;
    long conditionalAccepted = 0;
    long softRecoveries = 0;
    long conditionalRecoveries = 0;
    int softWins = 0;
    int strictWins = 0;
    int ties = 0;
    int softInfeasibleReturns = 0;
    int conditionalInfeasibleReturns = 0;
    for (const Row& row : rows) {
        strictGap += gap(row.strict.cost, row.optimum);
        softGap += gap(row.soft.cost, row.optimum);
        conditionalGap += gap(row.conditional.cost, row.optimum);
        softAccepted += row.soft.softStats.acceptedInfeasibleMoves;
        conditionalAccepted += row.conditional.softStats.acceptedInfeasibleMoves;
        softRecoveries += row.soft.softStats.feasibleRecoveries;
        conditionalRecoveries += row.conditional.softStats.feasibleRecoveries;
        softInfeasibleReturns += row.soft.feasible ? 0 : 1;
        conditionalInfeasibleReturns += row.conditional.feasible ? 0 : 1;
        if (row.soft.cost < row.strict.cost - 1e-9)
            ++softWins;
        else if (row.strict.cost < row.soft.cost - 1e-9)
            ++strictWins;
        else
            ++ties;
    }
    const double count = static_cast<double>(rows.size());
    std::cout << "\nRESUMO\n"
              << "GAP GLS estrita: " << strictGap / count << "%\n"
              << "GAP GLS com SLA suave: " << softGap / count << "%\n"
              << "GAP GLS condicional: " << conditionalGap / count << "%\n"
              << "SLA suave V/E/D: " << softWins << '/' << ties << '/' << strictWins << '\n'
              << "Suave aceitos/retornos: " << softAccepted
              << '/' << softRecoveries << '\n'
              << "Condicional aceitos/retornos: " << conditionalAccepted
              << '/' << conditionalRecoveries << '\n'
              << "Respostas finais inviáveis (suave/condicional): "
              << softInfeasibleReturns << '/' << conditionalInfeasibleReturns << '\n';
}

}  // namespace

int main() {
    const int repetitions = envInt("SSO_REPETITIONS", 3);
    const int baseSeed = envInt("SSO_SEED", 20260818);
    const double budgetSeconds = envDouble("SSO_TIME_SECONDS", 0.55);
    const double probabilityBeta = envDouble("SSO_PROBABILITY_BETA", 0.05);
    const double softTailFraction = envDouble("SSO_SOFT_TAIL_FRACTION", 0.20);
    const double proximityFraction = envDouble("SSO_PROXIMITY_FRACTION", 0.25);
    if (softTailFraction > 1.0)
        throw std::invalid_argument("SSO_SOFT_TAIL_FRACTION deve ser <= 1");
    const std::string outputPath = envString(
        "SSO_EXPERIMENT_OUTPUT",
        "data/experiments/gls_soft_sla_optimized_comparison.csv");
    const std::vector<int> instances = selectedInstances();

    std::cout << "GLS otimizada: estrita x suave x condicional\n"
              << "Instâncias: " << instances.size()
              << " | repetições: " << repetitions
              << " | tempo: " << budgetSeconds << " s"
              << " | beta: " << probabilityBeta
              << " | cauda suave: " << softTailFraction
              << " | proximidade: " << proximityFraction << '\n';

    std::vector<Row> rows;
    for (int instanceId : instances) {
        InstanceMatrix matrix = readInstance(instanceId);
        loadOptimalCost(matrix, instanceId);
        for (int repetition = 0; repetition < repetitions; ++repetition) {
            const unsigned seed = static_cast<unsigned>(
                baseSeed + instanceId * 1009 + repetition * 9176);
            RandomUtil::setSeed(seed);
            const Allocation initial = commonInitialSolution(matrix, 0.4);

            Row row;
            row.instance = instanceId;
            row.repetition = repetition + 1;
            row.seed = seed;
            row.optimum = matrix.getOptimalCost();
            row.initialCost = initial.getCurrentCost();
            row.strict = runStrictGls(
                initial, matrix, budgetSeconds, probabilityBeta);
            row.soft = runSoftSlaGls(
                initial, matrix, budgetSeconds, probabilityBeta);
            row.conditional = runConditionalGls(
                initial, matrix, budgetSeconds, probabilityBeta,
                softTailFraction, proximityFraction);
            rows.push_back(row);

            std::cout << "Instância " << std::setw(3) << instanceId
                      << " | r " << repetition + 1
                      << " | estrita " << row.strict.cost
                      << " | suave " << row.soft.cost
                      << " | condicional " << row.conditional.cost
                      << " | inviáveis S/C "
                      << row.soft.softStats.acceptedInfeasibleMoves << '/'
                      << row.conditional.softStats.acceptedInfeasibleMoves
                      << '\n';
        }
    }
    writeCsv(outputPath, rows, budgetSeconds, probabilityBeta,
             softTailFraction, proximityFraction);
    printSummary(rows);
    std::cout << "Resultados: " << outputPath << '\n';
    return 0;
}
