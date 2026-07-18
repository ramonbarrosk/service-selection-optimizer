// Harness de experimento controlado para isolar o efeito do GRASP adaptativo.
//
// Compila DUAS variantes do MESMO código:
//   - ADAPTIVE  (padrão)          : w = min(0.5, restartStreak/5)
//   - BASELINE  (-DDISABLE_ADAPTIVE): w = 0  (construtivo sempre guloso por custo)
//
// Cada execução usa um seed fixo (= índice da run), então ADAPTIVE e BASELINE
// veem EXATAMENTE a mesma sequência aleatória -> comparação pareada e justa.
// Orçamento por run = nº fixo de iterações ILS (sem limite de tempo), para que
// o único fator que varia entre as variantes seja o construtivo adaptativo.

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <chrono>

#include "basic/Allocation.h"
#include "instance/InstanceMatrix.hpp"
#include "metaheuristic/ILS.hpp"
#include "enum/ImprovementCondition.h"
#include "enum/ImprovementHeuristic.h"
#include "enum/ImprovementMode.h"
#include "enum/PertubationMode.h"
#include "enum/ProbabilityScenario.h"
#include "enum/SearchMode.h"
#include "util/RandomUtil.hpp"

using std::string;
using std::vector;
using std::cout;
using std::endl;

#ifdef INSTRUMENT
long g_restartBranch = 0, g_divePromising = 0, g_diveFeasible = 0, g_adaptiveWpos = 0, g_maxStreak = 0;
long g_improveTotal = 0, g_improveAfterRestart = 0, g_seenRestart = 0;
long g_fallbackGreedy = 0, g_fallbackAdaptive = 0;
#endif

static double nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static InstanceMatrix readInstance(int n) {
    string path = "data/instances/Instance_10_10_" + std::to_string(n);
    std::ifstream file(path);
    if (!file.is_open()) throw std::runtime_error("Cannot open: " + path);

    bool initLine = true;
    InstanceMatrix instance;
    string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        vector<string> tokens; string tok;
        while (iss >> tok) tokens.push_back(tok);
        if (tokens.empty()) continue;
        if (initLine) {
            instance = InstanceMatrix(std::stoi(tokens[2]), std::stoi(tokens[1]),
                                      std::stoi(tokens[3]), std::stoi(tokens[5]),
                                      std::stod(tokens[4]), std::stoi(tokens[6]));
            initLine = false;
        } else {
            if (tokens[0] == "r")
                instance.setResourceConsumption(std::stoi(tokens[1]) - 1, std::stoi(tokens[2]));
            else if (tokens[0] == "p")
                instance.setSlaViolationProbability(std::stoi(tokens[1]) - 1, std::stod(tokens[2]));
            else if (tokens[0] == "c")
                instance.setTaskCost(std::stoi(tokens[2]) - 1, std::stoi(tokens[1]) - 1, std::stoi(tokens[3]));
        }
    }
    for (int i = 0; i < instance.getNumberOfServices(); i++)
        instance.setServResourceCapacity(i, instance.getVres());
    instance.setInstanceName("Instance_10_10_" + std::to_string(n));
    return instance;
}

static void loadOptimal(InstanceMatrix& instance, int n) {
    std::ifstream f("data/Log/Instance_10_10_" + std::to_string(n));
    if (!f.is_open()) return;
    string lastLine, l;
    while (std::getline(f, l)) lastLine = l;
    auto pos = lastLine.rfind("= ");
    if (pos != string::npos) instance.setOptimalCost(std::stoi(lastLine.substr(pos + 2)));
}

int main(int argc, char** argv) {
    int    instanceNum = (argc > 1) ? std::stoi(argv[1]) : 100;
    int    NRUNS       = (argc > 2) ? std::stoi(argv[2]) : 30;
    int    IT_MAX      = (argc > 3) ? std::stoi(argv[3]) : 10000;

#ifdef DISABLE_ADAPTIVE
    const char* variant = "BASELINE";
#else
    const char* variant = "ADAPTIVE";
#endif

    InstanceMatrix instance = readInstance(instanceNum);
    loadOptimal(instance, instanceNum);
    int optimal = instance.getOptimalCost();

    cout << "# variant=" << variant
         << " instance=" << instance.getInstanceName()
         << " optimal=" << optimal
         << " NRUNS=" << NRUNS << " IT_MAX=" << IT_MAX << endl;
    cout << "run seed best timeMs" << endl;

    vector<double> bests;
    int reachedOpt = 0;
    double sumTime = 0;

    for (int r = 0; r < NRUNS; ++r) {
        RandomUtil::setSeed(static_cast<unsigned>(r));  // pareado entre variantes
#ifdef INSTRUMENT
        g_seenRestart = 0;  // reset por run
#endif
        double t0 = nowMs();
        ILS ils;
        Allocation a = ils.ILSWithRestart(
            instance, 0.2, IT_MAX, t0,
            ProbabilityScenario::Ps,
            ImprovementHeuristic::COST_IMPROVEMENT,
            SearchMode::LOCAL_SEARCH,
            ImprovementCondition::BEST_IMPROVEMENT,
            ImprovementMode::MOVE,
            PerturbationMode::MOVE);
        double dt = nowMs() - t0;
        double best = a.getCurrentCost();
        bests.push_back(best);
        sumTime += dt;
        if (optimal > 0 && best == (double)optimal) reachedOpt++;
        cout << r << " " << r << " " << best << " " << (long)dt << endl;
    }

    double mn = *std::min_element(bests.begin(), bests.end());
    double mean = 0; for (double b : bests) mean += b; mean /= bests.size();
    double var = 0; for (double b : bests) var += (b - mean) * (b - mean); var /= bests.size();
    double sd = std::sqrt(var);

    cout << "# SUMMARY variant=" << variant
         << " min=" << mn
         << " mean=" << mean
         << " sd=" << sd
         << " reachedOpt=" << reachedOpt << "/" << NRUNS
         << " gapMinPct=" << (optimal > 0 ? 100.0 * (mn - optimal) / optimal : 0)
         << " gapMeanPct=" << (optimal > 0 ? 100.0 * (mean - optimal) / optimal : 0)
         << " avgTimeMs=" << (sumTime / NRUNS)
         << endl;
#ifdef INSTRUMENT
    cout << "# INSTRUMENT restartBranchHits=" << g_restartBranch
         << " divePromising=" << g_divePromising
         << " diveReturnedFeasible=" << g_diveFeasible
         << " adaptiveCalls_w>0=" << g_adaptiveWpos
         << " maxRestartStreak=" << g_maxStreak
         << "  (total over " << NRUNS << " runs)" << endl;
    cout << "# INSTRUMENT globalImprovements=" << g_improveTotal
         << " improvementsAfterFirstRestart=" << g_improveAfterRestart
         << "  -> restart contributed " << g_improveAfterRestart << " of " << g_improveTotal
         << " improvements" << endl;
    cout << "# INSTRUMENT fallbackToProbBased: greedy(w=0)=" << g_fallbackGreedy
         << " adaptive(w>0)=" << g_fallbackAdaptive
         << "  (both fallbacks return the SAME deterministic solution)" << endl;
#endif
    return 0;
}
