#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
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
#include "Service.h"
#include "SolutionValidator.hpp"
#include "Task.h"

namespace fs = std::filesystem;

namespace {

constexpr int kRoundsPerCycle = 30;
const std::vector<int> kDifficultInstances = {28, 100, 128, 129, 147};

double nowMs() {
    using namespace std::chrono;
    return static_cast<double>(duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count());
}

int envInt(const char* name, int fallback) {
    const char* raw = std::getenv(name);
    if (!raw || !*raw)
        return fallback;
    const int value = std::stoi(raw);
    if (value <= 0)
        throw std::invalid_argument(std::string(name) + " deve ser positivo");
    return value;
}

double envDouble(const char* name, double fallback) {
    const char* raw = std::getenv(name);
    if (!raw || !*raw)
        return fallback;
    const double value = std::stod(raw);
    if (value <= 0.0)
        throw std::invalid_argument(std::string(name) + " deve ser positivo");
    return value;
}

std::string envString(const char* name, const std::string& fallback) {
    const char* raw = std::getenv(name);
    return raw && *raw ? raw : fallback;
}

// Escala do lambda da GLS; 0.3 é o valor promovido e SSO_ALPHA permite a varredura.
const double kGlsAlpha = envDouble("SSO_ALPHA", 0.3);

std::vector<int> selectedInstances() {
    const char* explicitInstances = std::getenv("SSO_INSTANCES");
    if (explicitInstances && *explicitInstances) {
        std::vector<int> instances;
        std::istringstream input(explicitInstances);
        std::string token;
        while (std::getline(input, token, ',')) {
            const int instance = std::stoi(token);
            if (instance <= 0)
                throw std::invalid_argument("SSO_INSTANCES contém identificador inválido");
            instances.push_back(instance);
        }
        std::sort(instances.begin(), instances.end());
        instances.erase(std::unique(instances.begin(), instances.end()),
                        instances.end());
        if (instances.empty())
            throw std::invalid_argument("SSO_INSTANCES não pode ser vazio");
        return instances;
    }

    const std::string selection = envString("SSO_INSTANCE_SET", "difficult5");
    if (selection == "difficult5")
        return kDifficultInstances;
    if (selection != "all")
        throw std::invalid_argument(
            "SSO_INSTANCE_SET deve ser 'difficult5' ou 'all'");

    std::vector<int> instances;
    const std::string prefix = "Instance_10_10_";
    for (const fs::directory_entry& entry : fs::directory_iterator("data/instances")) {
        if (!entry.is_regular_file())
            continue;
        const std::string name = entry.path().filename().string();
        if (name.rfind(prefix, 0) != 0)
            continue;
        instances.push_back(std::stoi(name.substr(prefix.size())));
    }
    std::sort(instances.begin(), instances.end());
    if (instances.empty())
        throw std::runtime_error("Nenhuma instância encontrada em data/instances");
    return instances;
}

std::vector<double> selectedBudgets() {
    const char* raw = std::getenv("SSO_TIME_CURVE");
    if (!raw || !*raw)
        return {envDouble("SSO_TIME_SECONDS", 0.55)};
    std::vector<double> budgets;
    std::istringstream input(raw);
    std::string token;
    while (std::getline(input, token, ',')) {
        const double budget = std::stod(token);
        if (budget <= 0.0)
            throw std::invalid_argument("SSO_TIME_CURVE contém tempo inválido");
        budgets.push_back(budget);
    }
    if (budgets.empty())
        throw std::invalid_argument("SSO_TIME_CURVE não pode ser vazio");
    return budgets;
}

InstanceMatrix readInstance(int id) {
    const std::string path =
        "data/instances/Instance_10_10_" + std::to_string(id);
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
                std::stoi(tokens[2]), std::stoi(tokens[1]),
                std::stoi(tokens[3]), std::stoi(tokens[5]),
                std::stod(tokens[4]), std::stoi(tokens[6]));
            firstLine = false;
        } else if (tokens[0] == "r") {
            instance.setResourceConsumption(
                std::stoi(tokens[1]) - 1, std::stoi(tokens[2]));
        } else if (tokens[0] == "p") {
            instance.setSlaViolationProbability(
                std::stoi(tokens[1]) - 1, std::stod(tokens[2]));
        } else if (tokens[0] == "c") {
            instance.setTaskCost(
                std::stoi(tokens[2]) - 1, std::stoi(tokens[1]) - 1,
                std::stoi(tokens[3]));
        }
    }
    for (int service = 0; service < instance.getNumberOfServices(); ++service)
        instance.setServResourceCapacity(service, instance.getVres());
    instance.setInstanceName("Instance_10_10_" + std::to_string(id));
    return instance;
}

void loadOptimum(InstanceMatrix& instance, int id) {
    std::ifstream file("data/Log/Instance_10_10_" + std::to_string(id));
    if (!file.is_open())
        throw std::runtime_error("Log de referência ausente para a instância "
                                 + std::to_string(id));
    std::string line;
    std::string lastLine;
    while (std::getline(file, line)) {
        if (line.rfind("Total", 0) == 0) {
            const auto equalTotal = line.find('=');
            const auto seconds = line.find("sec");
            if (equalTotal != std::string::npos) {
                const std::string value = line.substr(
                    equalTotal + 1,
                    seconds == std::string::npos
                        ? std::string::npos : seconds - equalTotal - 1);
                instance.setOptimalExecTime(std::stod(value));
            }
        }
        if (!line.empty())
            lastLine = line;
    }
    const auto equal = lastLine.rfind("= ");
    if (equal == std::string::npos)
        throw std::runtime_error("Ótimo ausente no log da instância "
                                 + std::to_string(id));
    instance.setOptimalCost(std::stoi(lastLine.substr(equal + 2)));
}

// As cinco instâncias difíceis fazem o construtivo guloso cair neste fallback.
// Usá-lo diretamente garante a mesma solução inicial para todas as variantes.
Allocation probabilityInitialSolution(const InstanceMatrix& matrix) {
    SolutionValidator validator;
    ProbabilityScenario scenario = ProbabilityScenario::Ps;
    Allocation allocation;
    std::vector<int> services(matrix.getNumberOfServices());
    std::iota(services.begin(), services.end(), 0);
    std::stable_sort(services.begin(), services.end(),
        [&](int first, int second) {
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
            throw std::runtime_error("Não foi possível construir solução viável");
    }
    return allocation;
}

bool isFeasible(const Allocation& solution, const InstanceMatrix& matrix) {
    SolutionValidator validator;
    ProbabilityScenario scenario = ProbabilityScenario::Ps;
    Allocation copy = solution;
    return validator.isFeasible(
        matrix, copy, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(),
        scenario, true);
}

struct Variant {
    std::string key;
    std::string label;
    GuidedLocalSearchOptions options;
    bool conditionalOscillation = false;
};

std::vector<Variant> variants() {
    GuidedLocalSearchOptions gls;
    gls.useGfls = false;
    gls.useIncrementalFeasibility = false;
    gls.useServiceReplacementNeighborhood = false;
    gls.calibrateLambdaAfterInitialDescent = false;
    gls.useRegretUtility = false;
    gls.maximumPenalizedFeatures = 0;

    GuidedLocalSearchOptions gfls = gls;
    gfls.useGfls = true;

    GuidedLocalSearchOptions incremental = gfls;
    incremental.useIncrementalFeasibility = true;

    GuidedLocalSearchOptions serviceReplacement = incremental;
    serviceReplacement.useServiceReplacementNeighborhood = true;

    GuidedLocalSearchOptions localLambda = serviceReplacement;
    localLambda.calibrateLambdaAfterInitialDescent = true;

    GuidedLocalSearchOptions regret = localLambda;
    regret.useRegretUtility = true;

    GuidedLocalSearchOptions topOne = regret;
    topOne.maximumPenalizedFeatures = 1;

    std::vector<Variant> result = {
        {"gls", "GLS", gls, false},
        {"gfls", "+ GFLS", gfls, false},
        {"incremental", "+ avaliação incremental", incremental, false},
        {"service_replace", "+ substituição de serviço", serviceReplacement, false},
        {"lambda_local", "+ lambda local", localLambda, false},
        {"regret", "+ utilidade por arrependimento", regret, false},
        {"top1", "+ penalização top-1", topOne, false},
        {"osc_cond", "+ oscilação condicional", topOne, true},
    };
    const char* factorial = std::getenv("SSO_FACTORIAL");
    if (factorial && *factorial && std::string(factorial) != "0") {
        result.clear();
        for (int mask = 0; mask < 16; ++mask) {
            GuidedLocalSearchOptions options;
            options.useGfls = true;
            options.useIncrementalFeasibility = (mask & 1) != 0;
            options.useServiceReplacementNeighborhood = (mask & 2) != 0;
            options.calibrateLambdaAfterInitialDescent = (mask & 4) != 0;
            options.useRegretUtility = (mask & 8) != 0;
            options.maximumPenalizedFeatures = 0;
            const std::string key =
                "i" + std::to_string(options.useIncrementalFeasibility)
                + "_s" + std::to_string(options.useServiceReplacementNeighborhood)
                + "_l" + std::to_string(options.calibrateLambdaAfterInitialDescent)
                + "_r" + std::to_string(options.useRegretUtility);
            const std::string label =
                "I=" + std::to_string(options.useIncrementalFeasibility)
                + " S=" + std::to_string(options.useServiceReplacementNeighborhood)
                + " L=" + std::to_string(options.calibrateLambdaAfterInitialDescent)
                + " R=" + std::to_string(options.useRegretUtility);
            result.push_back({key, label, options, false});
        }
    }
    const char* selected = std::getenv("SSO_VARIANTS");
    if (!selected || !*selected)
        return result;
    std::vector<Variant> filtered;
    std::istringstream input(selected);
    std::string key;
    while (std::getline(input, key, ',')) {
        const auto found = std::find_if(
            result.begin(), result.end(),
            [&](const Variant& variant) { return variant.key == key; });
        if (found == result.end())
            throw std::invalid_argument("Variante desconhecida: " + key);
        filtered.push_back(*found);
    }
    return filtered;
}

struct Result {
    double cost = std::numeric_limits<double>::infinity();
    double elapsedMs = 0.0;
    double timeToBestMs = 0.0;
    bool feasible = false;
    GuidedLocalSearchStats stats;
    int oscillationCalls = 0;
    int successfulOscillations = 0;
};

Result runVariant(const Allocation& initial, const InstanceMatrix& matrix,
                  const Variant& variant, double budgetSeconds) {
    const double start = nowMs();
    const double deadline = start + budgetSeconds * 1000.0;
    Allocation current = initial;
    Allocation globalBest = initial;
    GuidedLocalSearcher guidedSearcher;
    GenericSearcher genericSearcher;
    ProbabilityScenario scenario = ProbabilityScenario::Ps;
    int stagnantCycles = 0;
    int oscillationCooldown = 0;
    int oscillationCalls = 0;
    int successfulOscillations = 0;
    double timeToBestMs = 0.0;

    while (nowMs() < deadline) {
        const double costBefore = globalBest.getCurrentCost();
        const GuidedLocalSearchStats before = guidedSearcher.stats();
        current = guidedSearcher.improve(
            current, matrix, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(),
            scenario, kGlsAlpha, kRoundsPerCycle, true, deadline,
            variant.options);
        if (current.getCurrentCost() < globalBest.getCurrentCost()) {
            globalBest = current;
            timeToBestMs = nowMs() - start;
        }

        if (globalBest.getCurrentCost() < costBefore - 1e-9)
            stagnantCycles = 0;
        else
            ++stagnantCycles;
        if (oscillationCooldown > 0)
            --oscillationCooldown;

        if (variant.conditionalOscillation && nowMs() < deadline) {
            const GuidedLocalSearchStats after = guidedSearcher.stats();
            const long long capacityBlocked =
                after.rejectedByCapacity - before.rejectedByCapacity;
            const long long slaBlocked = after.rejectedBySla - before.rejectedBySla;
            const long long serviceBlocked =
                after.rejectedByServiceLimit - before.rejectedByServiceLimit;
            const long long totalBlocked =
                capacityBlocked + slaBlocked + serviceBlocked;
            const double capacityFraction = totalBlocked > 0
                ? capacityBlocked / static_cast<double>(totalBlocked) : 0.0;

            // A oscilação só é acionada se a busca estagnou e a capacidade foi
            // responsável pela maioria dos movimentos promissores rejeitados.
            if (stagnantCycles >= 2 && oscillationCooldown == 0
                    && capacityBlocked >= 100 && capacityFraction >= 0.60) {
                ++oscillationCalls;
                const double beforeOscillation = current.getCurrentCost();
                const bool improved = genericSearcher.oscillationImprovement(
                    current, matrix, matrix.getVmax(), matrix.getSmax(),
                    matrix.getPmax(), scenario, 8, deadline);
                if (improved && current.getCurrentCost() < beforeOscillation - 1e-9) {
                    ++successfulOscillations;
                    stagnantCycles = 0;
                    if (current.getCurrentCost() < globalBest.getCurrentCost()) {
                        globalBest = current;
                        timeToBestMs = nowMs() - start;
                    }
                }
                oscillationCooldown = improved ? 2 : 4;
            }
        }

        if (globalBest.getCurrentCost() <= matrix.getOptimalCost())
            break;
    }

    return {
        globalBest.getCurrentCost(), nowMs() - start, timeToBestMs,
        isFeasible(globalBest, matrix), guidedSearcher.stats(),
        oscillationCalls, successfulOscillations
    };
}

void validateIncrementalEvaluation(const Allocation& initial,
                                   const InstanceMatrix& matrix) {
    GuidedLocalSearchOptions complete;
    complete.useGfls = true;
    complete.useIncrementalFeasibility = false;
    complete.calibrateLambdaAfterInitialDescent = false;
    complete.useRegretUtility = false;
    complete.maximumPenalizedFeatures = 0;
    GuidedLocalSearchOptions incremental = complete;
    incremental.useIncrementalFeasibility = true;
    ProbabilityScenario scenario = ProbabilityScenario::Ps;
    GuidedLocalSearcher completeSearcher;
    GuidedLocalSearcher incrementalSearcher;
    const Allocation completeResult = completeSearcher.improve(
        initial, matrix, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(),
        scenario, kGlsAlpha, kRoundsPerCycle, false,
        std::numeric_limits<double>::infinity(), complete);
    const Allocation incrementalResult = incrementalSearcher.improve(
        initial, matrix, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(),
        scenario, kGlsAlpha, kRoundsPerCycle, false,
        std::numeric_limits<double>::infinity(), incremental);
    if (std::abs(completeResult.getCurrentCost()
                 - incrementalResult.getCurrentCost()) > 1e-9) {
        throw std::runtime_error(
            "Avaliação incremental divergiu da validação completa na instância "
            + matrix.getInstanceName());
    }
}

double gapPercent(double cost, int optimum) {
    return 100.0 * (cost - optimum) / optimum;
}

struct Row {
    int instance = 0;
    int repetition = 0;
    unsigned seed = 0;
    int optimum = 0;
    double budgetSeconds = 0.0;
    double initialCost = 0.0;
    Variant variant;
    Result result;
};

void writeCsv(const std::string& outputPath, const std::vector<Row>& rows) {
    const fs::path path(outputPath);
    if (path.has_parent_path())
        fs::create_directories(path.parent_path());
    std::ofstream output(path);
    if (!output.is_open())
        throw std::runtime_error("Não foi possível criar: " + outputPath);

    output << "instance,repetition,seed,optimum,budget_seconds,initial_cost,"
              "variant,label,incremental_enabled,service_replace_enabled,"
              "lambda_local_enabled,regret_enabled,"
              "cost,gap_pct,time_ms,feasible,candidates,rounds,"
              "moves,swaps,rejected_capacity,rejected_sla,rejected_smax,lambda,"
              "service_replacements,oscillation_calls,successful_oscillations,"
              "time_to_best_ms\n";
    output << std::fixed << std::setprecision(6);
    for (const Row& row : rows) {
        const auto& stats = row.result.stats;
        output << row.instance << ',' << row.repetition << ',' << row.seed << ','
               << row.optimum << ',' << row.budgetSeconds << ','
               << row.initialCost << ',' << row.variant.key << ','
               << row.variant.label << ','
               << row.variant.options.useIncrementalFeasibility << ','
               << row.variant.options.useServiceReplacementNeighborhood << ','
               << row.variant.options.calibrateLambdaAfterInitialDescent << ','
               << row.variant.options.useRegretUtility << ','
               << row.result.cost << ','
               << gapPercent(row.result.cost, row.optimum) << ','
               << row.result.elapsedMs << ',' << row.result.feasible << ','
               << stats.evaluatedCandidates << ','
               << stats.completedPenaltyRounds << ',' << stats.acceptedMoves << ','
               << stats.acceptedSwaps << ',' << stats.rejectedByCapacity << ','
               << stats.rejectedBySla << ',' << stats.rejectedByServiceLimit << ','
               << stats.lambda << ',' << stats.acceptedServiceReplacements << ','
               << row.result.oscillationCalls << ','
               << row.result.successfulOscillations << ','
               << row.result.timeToBestMs << '\n';
    }
}

}  // namespace

int main() {
    const int repetitions = envInt("SSO_REPETITIONS", 3);
    const int baseSeed = envInt("SSO_SEED", 20260831);
    const std::vector<double> budgets = selectedBudgets();
    const char* articleRaw = std::getenv("SSO_ARTICLE_PROTOCOL");
    const bool articleProtocol = articleRaw && *articleRaw
        && std::string(articleRaw) != "0";
    const char* independentRaw = std::getenv("SSO_INDEPENDENT_REPETITIONS");
    const bool independentRepetitions = independentRaw && *independentRaw
        && std::string(independentRaw) != "0";
    const std::string outputPath = envString(
        "SSO_EXPERIMENT_OUTPUT",
        "data/experiments/gls_improvements_difficult5.csv");
    const std::vector<Variant> experimentVariants = variants();
    const std::vector<int> experimentInstances = selectedInstances();

    std::cout << "Experimento GLS/GFLS\n"
              << "Instâncias: " << experimentInstances.size()
              << " | repetições: " << repetitions
              << " | orçamentos:";
    for (double budget : budgets)
        std::cout << ' ' << budget << "s";
    std::cout << (articleProtocol
        ? " por variante (orçamento fixo de 10s por instância)\n"
        : " por variante\n");

    std::vector<Row> rows;
    rows.reserve(experimentInstances.size() * experimentVariants.size()
                 * repetitions * budgets.size());
    for (double budgetSeconds : budgets) {
      std::cout << "\n=== Orçamento " << budgetSeconds << " s ===\n";
      for (int instanceId : experimentInstances) {
        InstanceMatrix matrix = readInstance(instanceId);
        loadOptimum(matrix, instanceId);
        const double articleBudget = 10.0;
        const double runBudget = articleProtocol ? articleBudget : budgetSeconds;
        const Allocation sharedInitial = probabilityInitialSolution(matrix);
        validateIncrementalEvaluation(sharedInitial, matrix);
        std::cout << "\nInstância " << instanceId
                  << " | ótimo " << matrix.getOptimalCost()
                  << " | inicial " << sharedInitial.getCurrentCost() << '\n';

        for (int repetition = 1; repetition <= repetitions; ++repetition) {
            const unsigned seed = static_cast<unsigned>(
                baseSeed + instanceId * 1009 + repetition * 9176);
            if (independentRepetitions)
                RandomUtil::setSeed(seed);
            const Allocation initial = independentRepetitions
                ? probabilityInitialSolution(matrix) : sharedInitial;
            if (independentRepetitions)
                validateIncrementalEvaluation(initial, matrix);
            for (const Variant& variant : experimentVariants) {
                const Result result = runVariant(
                    initial, matrix, variant, runBudget);
                rows.push_back({
                    instanceId, repetition, seed, matrix.getOptimalCost(),
                    runBudget, initial.getCurrentCost(), variant, result
                });
                std::cout << "  rep " << repetition << " | "
                          << std::left << std::setw(29) << variant.label
                          << " custo " << std::right << std::setw(5)
                          << result.cost << " | GAP " << std::fixed
                          << std::setprecision(2)
                          << gapPercent(result.cost, matrix.getOptimalCost())
                          << "% | rodadas " << result.stats.completedPenaltyRounds
                          << '\n';
            }
        }
      }
    }
    writeCsv(outputPath, rows);
    std::cout << "\nCSV salvo em " << outputPath << '\n';
    return 0;
}
