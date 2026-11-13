// Executa a GLS promovida sobre as instâncias do artigo de referência.
//
// Protocolo: cada instância roda SSO_REPETITIONS vezes (padrão 3), cada uma com
// orçamento fixo de SSO_TIME_SECONDS (padrão 10 s), encerrando antes se alcançar
// o ótimo conhecido. SSO_INSTANCES=1,28,100 restringe as instâncias executadas.
// SSO_CSV=arquivo.csv grava uma linha por repetição, no formato do ArticleResult Java.
// SSO_INSTANCE_DIR e SSO_LOG_DIR trocam o conjunto de instâncias. Instâncias sem log
// do método exato usam o limite inferior trivial como referência: alcançá-lo prova
// o ótimo, e o GAP passa a ser medido contra esse limite.
// SSO_GLS_* trocam os parâmetros da GLS (veja glsParametersFromEnv em GlsSolver.hpp).

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "GlsSolver.hpp"
#include "InitialSolution.hpp"
#include "InstanceMatrix.hpp"
#include "InstanceReader.hpp"

using std::cout;
using std::endl;
using std::string;
using std::vector;

struct InstanceSummary {
    // Estatísticas agregadas das repetições de uma única instância.
    string instanceName;
    int optimalCost = 0;
    double optimalExecTime = 0.0;
    double bestCost = 0.0;
    double meanCost = 0.0;
    double meanTimeToBestMs = 0.0;
    int reachedOptimal = 0;
};

static int envInt(const char* name, int fallback) {
    const char* value = std::getenv(name);
    if (!value || !*value)
        return fallback;
    const int parsed = std::stoi(value);
    if (parsed <= 0)
        throw std::invalid_argument(string(name) + " deve ser positivo");
    return parsed;
}

static double envDouble(const char* name, double fallback) {
    const char* value = std::getenv(name);
    if (!value || !*value)
        return fallback;
    const double parsed = std::stod(value);
    if (parsed <= 0.0)
        throw std::invalid_argument(string(name) + " deve ser positivo");
    return parsed;
}

static vector<int> selectedInstances() {
    const char* raw = std::getenv("SSO_INSTANCES");
    if (!raw || !*raw)
        return listInstanceIds();
    vector<int> ids;
    std::istringstream input(raw);
    string token;
    while (std::getline(input, token, ','))
        ids.push_back(std::stoi(token));
    return ids;
}

int main() {
    const double programStart = glsNowMs();
    const int repetitions = envInt("SSO_REPETITIONS", 3);
    const double budgetSeconds = envDouble("SSO_TIME_SECONDS", 10.0);
    const vector<int> instanceIds = selectedInstances();
    const char* csvPath = std::getenv("SSO_CSV");
    std::ofstream csv;
    if (csvPath && *csvPath) {
        csv.open(csvPath);
        if (!csv)
            throw std::runtime_error(string("Não foi possível abrir ") + csvPath);
        csv << "instance,repetition,optimum,cost,gap_pct,reached_optimal,time_to_best_ms,elapsed_ms,budget_seconds,reference\n";
    }

    const GlsParameters parameters = glsParametersFromEnv();
    cout << "CONFIG algorithm=GLS repetitions=" << repetitions
         << " timeSeconds=" << budgetSeconds
         << " " << describeGlsParameters(parameters) << endl;

    vector<InstanceSummary> summaries;
    for (int id : instanceIds) {
        InstanceMatrix instance = readInstance(id);
        const bool exactReference = loadReferenceLog(instance, id);
        if (!exactReference)
            instance.setOptimalCost(trivialLowerBound(instance));
        const Allocation initial = buildInitialSolution(instance);
        const bool hasOptimal = instance.getOptimalCost() > 0;

        InstanceSummary summary;
        summary.instanceName = instance.getInstanceName();
        summary.optimalCost = instance.getOptimalCost();
        summary.optimalExecTime = instance.getOptimalExecTime();
        cout << "Executing instance " << id << endl;

        for (int repetition = 0; repetition < repetitions; ++repetition) {
            cout << "r " << repetition << endl;
            const GlsRunResult run = runGuidedLocalSearch(
                initial, instance, budgetSeconds, parameters.options,
                parameters.alpha, parameters.roundsPerCycle);
            const double cost = run.best.getCurrentCost();
            const bool optimal = hasOptimal && cost <= instance.getOptimalCost();

            // Sem ótimo, o tempo até a melhor solução conta como o orçamento inteiro,
            // como no relatório do artigo de referência.
            const double timeToBestMs = optimal ? run.timeToBestMs : run.elapsedMs;
            if (repetition == 0 || cost < summary.bestCost)
                summary.bestCost = cost;
            summary.meanCost += cost / repetitions;
            summary.meanTimeToBestMs += timeToBestMs / repetitions;
            if (optimal)
                ++summary.reachedOptimal;
            if (csv.is_open()) {
                const double optimum = instance.getOptimalCost();
                csv << id << "," << repetition + 1 << "," << instance.getOptimalCost() << ","
                    << cost << "," << 100.0 * (cost - optimum) / optimum << ","
                    << (optimal ? 1 : 0) << "," << timeToBestMs << "," << run.elapsedMs << ","
                    << budgetSeconds << "," << (exactReference ? "exact" : "lower_bound")
                    << std::endl;
            }
        }
        summaries.push_back(summary);
    }

    // Uma linha por instância e um resumo agregado do experimento.
    cout << "Instance | Optimal Cost | Optimal Time | Mean Best Cost | Best Cost | Mean time to Best | Reached Optimal" << endl;
    double meanBestCost = 0.0;
    int withKnownOptimal = 0;
    vector<string> optimalInstances;
    for (const InstanceSummary& summary : summaries) {
        const bool hasOptimal = summary.optimalCost > 0;
        cout << summary.instanceName << " "
             << summary.optimalCost << " "
             << summary.optimalExecTime << " "
             << summary.meanCost << " "
             << summary.bestCost << " "
             << summary.meanTimeToBestMs / 1000.0 << " "
             << (summary.reachedOptimal > 0
                 ? "YES (" + std::to_string(summary.reachedOptimal) + "/"
                     + std::to_string(repetitions) + ")"
                 : (hasOptimal ? "NO" : "N/A"))
             << endl;
        meanBestCost += summary.bestCost / summaries.size();
        if (hasOptimal) {
            ++withKnownOptimal;
            if (summary.reachedOptimal > 0)
                optimalInstances.push_back(summary.instanceName);
        }
    }

    cout << "\nMEAN BEST COSTS: " << meanBestCost << endl;
    cout << "REACHED OPTIMALL: " << optimalInstances.size() << "/"
         << withKnownOptimal << " instances" << endl;
    if (!optimalInstances.empty()) {
        cout << "INSTANCES THAT REACHED OPTIMAL:" << endl;
        for (const string& name : optimalInstances)
            cout << "  " << name << endl;
    }

    const double totalSeconds = (glsNowMs() - programStart) / 1000.0;
    const int totalMinutes = static_cast<int>(totalSeconds) / 60;
    cout << "TOTAL EXECUTION TIME: " << totalMinutes << "m "
         << totalSeconds - totalMinutes * 60 << "s" << endl;
    return 0;
}
