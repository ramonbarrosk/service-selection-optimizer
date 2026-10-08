#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Allocation.h"
#include "GlsSolver.hpp"
#include "GuidedLocalSearcher.hpp"
#include "InitialSolution.hpp"
#include "InstanceMatrix.hpp"
#include "InstanceReader.hpp"
#include "ProbabilityScenario.h"

namespace fs = std::filesystem;

namespace {

const std::vector<int> kDifficultInstances = {28, 100, 128, 129, 147};

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

// Escala do lambda da GLS; o padrão é o valor promovido e SSO_ALPHA permite a varredura.
const double kExperimentAlpha = envDouble("SSO_ALPHA", kGlsAlpha);

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

    return listInstanceIds();
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

void loadOptimum(InstanceMatrix& instance, int id) {
    if (!loadReferenceLog(instance, id))
        throw std::runtime_error("Log de referência ausente ou sem ótimo para a instância "
                                 + std::to_string(id));
}

struct Variant {
    std::string key;
    std::string label;
    GuidedLocalSearchOptions options;
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
        {"gls", "GLS", gls},
        {"gfls", "+ GFLS", gfls},
        {"incremental", "+ avaliação incremental", incremental},
        {"service_replace", "+ substituição de serviço", serviceReplacement},
        {"lambda_local", "+ lambda local", localLambda},
        {"regret", "+ utilidade por arrependimento", regret},
        {"top1", "+ penalização top-1", topOne},
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
            result.push_back({key, label, options});
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
};

Result runVariant(const Allocation& initial, const InstanceMatrix& matrix,
                  const Variant& variant, double budgetSeconds) {
    const GlsRunResult run = runGuidedLocalSearch(
        initial, matrix, budgetSeconds, variant.options, kExperimentAlpha);
    return {run.best.getCurrentCost(), run.elapsedMs, run.timeToBestMs,
            isFeasible(run.best, matrix), run.stats};
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
        scenario, kExperimentAlpha, kGlsRoundsPerCycle, false,
        std::numeric_limits<double>::infinity(), complete);
    const Allocation incrementalResult = incrementalSearcher.improve(
        initial, matrix, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(),
        scenario, kExperimentAlpha, kGlsRoundsPerCycle, false,
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
              "service_replacements,time_to_best_ms\n";
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
    const std::string outputPath = envString(
        "SSO_EXPERIMENT_OUTPUT",
        "experiments/06_gls_melhorias/results/gls_improvements_difficult5.csv");
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
        // A construção inicial é determinística: todas as repetições e variantes
        // partem da mesma solução.
        const Allocation initial = buildInitialSolution(matrix);
        validateIncrementalEvaluation(initial, matrix);
        std::cout << "\nInstância " << instanceId
                  << " | ótimo " << matrix.getOptimalCost()
                  << " | inicial " << initial.getCurrentCost() << '\n';

        for (int repetition = 1; repetition <= repetitions; ++repetition) {
            const unsigned seed = static_cast<unsigned>(
                baseSeed + instanceId * 1009 + repetition * 9176);
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
