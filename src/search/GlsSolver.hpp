#pragma once

#include <chrono>

#include "Allocation.h"
#include "GuidedLocalSearcher.hpp"
#include "InstanceMatrix.hpp"
#include "ProbabilityScenario.h"

// Parâmetros da GLS promovida: lambda = alpha * custoInicial / tarefas e
// 30 rodadas de penalização por ciclo, com penalidades preservadas entre ciclos.
inline constexpr double kGlsAlpha = 0.3;
inline constexpr int kGlsRoundsPerCycle = 30;

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
