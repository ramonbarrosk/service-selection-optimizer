#pragma once

#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "Allocation.h"
#include "GuidedLocalSearcher.hpp"
#include "InstanceMatrix.hpp"
#include "ProbabilityScenario.h"

// Parâmetros da GLS promovida: lambda = alpha * custoInicial / tarefas e
// 30 rodadas de penalização por ciclo, com penalidades preservadas entre ciclos.
inline constexpr double kGlsAlpha = 0.3;
inline constexpr int kGlsRoundsPerCycle = 30;

// Parâmetros ajustáveis da GLS. O padrão é a configuração promovida acima.
struct GlsParameters {
    double alpha = kGlsAlpha;
    int roundsPerCycle = kGlsRoundsPerCycle;
    GuidedLocalSearchOptions options;
};

inline const char* glsEnvValue(const char* name) {
    const char* value = std::getenv(name);
    return value && *value ? value : nullptr;
}

inline bool glsEnvFlag(const char* name, bool fallback) {
    const char* value = glsEnvValue(name);
    if (!value)
        return fallback;
    const std::string text = value;
    if (text != "0" && text != "1")
        throw std::invalid_argument(std::string(name) + " deve ser 0 ou 1");
    return text == "1";
}

// Lê SSO_GLS_ALPHA, SSO_GLS_ROUNDS, SSO_GLS_GFLS, SSO_GLS_REPLACEMENT,
// SSO_GLS_REGRET, SSO_GLS_CALIBRATE e SSO_GLS_MAX_PENALIZED; os ausentes ficam
// no padrão. É o que a calibração com o irace (fase 11) varia.
inline GlsParameters glsParametersFromEnv() {
    GlsParameters parameters;
    GuidedLocalSearchOptions& options = parameters.options;
    if (const char* value = glsEnvValue("SSO_GLS_ALPHA"))
        parameters.alpha = std::stod(value);
    if (const char* value = glsEnvValue("SSO_GLS_ROUNDS"))
        parameters.roundsPerCycle = std::stoi(value);
    if (const char* value = glsEnvValue("SSO_GLS_MAX_PENALIZED"))
        options.maximumPenalizedFeatures = std::stoi(value);
    options.useGfls = glsEnvFlag("SSO_GLS_GFLS", options.useGfls);
    options.useServiceReplacementNeighborhood = glsEnvFlag(
        "SSO_GLS_REPLACEMENT", options.useServiceReplacementNeighborhood);
    options.useRegretUtility = glsEnvFlag("SSO_GLS_REGRET", options.useRegretUtility);
    options.calibrateLambdaAfterInitialDescent = glsEnvFlag(
        "SSO_GLS_CALIBRATE", options.calibrateLambdaAfterInitialDescent);
    if (parameters.alpha <= 0.0 || parameters.roundsPerCycle <= 0
            || options.maximumPenalizedFeatures < 0)
        throw std::invalid_argument("Parâmetros da GLS inválidos");
    return parameters;
}

inline std::string describeGlsParameters(const GlsParameters& parameters) {
    const GuidedLocalSearchOptions& options = parameters.options;
    return "alpha=" + std::to_string(parameters.alpha)
        + " roundsPerCycle=" + std::to_string(parameters.roundsPerCycle)
        + " gfls=" + std::to_string(options.useGfls)
        + " replacement=" + std::to_string(options.useServiceReplacementNeighborhood)
        + " regret=" + std::to_string(options.useRegretUtility)
        + " calibrate=" + std::to_string(options.calibrateLambdaAfterInitialDescent)
        + " maxPenalized=" + std::to_string(options.maximumPenalizedFeatures);
}

struct GlsRunResult {
    Allocation best;
    double elapsedMs = 0.0;
    double timeToBestMs = 0.0;
    GuidedLocalSearchStats stats;
};

inline double glsNowMs() {
    using namespace std::chrono;
    return static_cast<double>(duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count());
}

// Executa ciclos da GLS até esgotar o orçamento ou alcançar o ótimo conhecido.
inline GlsRunResult runGuidedLocalSearch(
        const Allocation& initial, const InstanceMatrix& matrix,
        double budgetSeconds,
        const GuidedLocalSearchOptions& options = GuidedLocalSearchOptions{},
        double alpha = kGlsAlpha, int roundsPerCycle = kGlsRoundsPerCycle) {
    const double start = glsNowMs();
    const double deadline = start + budgetSeconds * 1000.0;
    Allocation current = initial;
    GlsRunResult result;
    result.best = initial;
    GuidedLocalSearcher guidedSearcher;
    ProbabilityScenario scenario = ProbabilityScenario::Ps;

    while (glsNowMs() < deadline) {
        current = guidedSearcher.improve(
            current, matrix, matrix.getVmax(), matrix.getSmax(), matrix.getPmax(),
            scenario, alpha, roundsPerCycle, true, deadline, options);
        if (current.getCurrentCost() < result.best.getCurrentCost()) {
            result.best = current;
            result.timeToBestMs = glsNowMs() - start;
        }
        if (matrix.getOptimalCost() > 0
                && result.best.getCurrentCost() <= matrix.getOptimalCost())
            break;
    }

    result.elapsedMs = glsNowMs() - start;
    result.stats = guidedSearcher.stats();
    return result;
}
