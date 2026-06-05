#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <map>
#include <vector>
#include <limits>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <stdexcept>

#include "basic/Allocation.h"
#include "instance/InstanceMatrix.hpp"
#include "metaheuristic/ILS.hpp"
#include "enum/ImprovementCondition.h"
#include "enum/ImprovementHeuristic.h"
#include "enum/ImprovementMode.h"
#include "enum/PertubationMode.h"
#include "enum/ProbabilityScenario.h"
#include "enum/SearchMode.h"

namespace fs = std::filesystem;
using std::cout;
using std::endl;
using std::string;
using std::vector;

struct AlgResult {
    double bestResult  = 0;
    double meanResult  = 0;
    double timeToBest  = 0;
    double countBests  = 0;
    string instanceName;
};

static double nowMs() {
    using namespace std::chrono;
    return static_cast<double>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

static InstanceMatrix readInstance(int instanceCont) {
    string path = "data/instances/Instance_10_10_" + std::to_string(instanceCont);
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open: " + path);

    bool initLine = true;
    InstanceMatrix instance;
    string line;

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        vector<string> tokens;
        string tok;
        while (iss >> tok) tokens.push_back(tok);
        if (tokens.empty()) continue;

        if (initLine) {
            int numberOfServices = std::stoi(tokens[1]);
            int numberOfTasks    = std::stoi(tokens[2]);
            int vMax             = std::stoi(tokens[3]);
            double pMax          = std::stod(tokens[4]);
            int Smax             = std::stoi(tokens[5]);
            int Vres             = std::stoi(tokens[6]);

            instance = InstanceMatrix(numberOfTasks, numberOfServices, vMax, Smax, pMax, Vres);
            initLine = false;
        } else {
            if (tokens[0] == "r") {
                instance.setResourceConsumption(std::stoi(tokens[1]) - 1, std::stoi(tokens[2]));
            } else if (tokens[0] == "p") {
                instance.setSlaViolationProbability(std::stoi(tokens[1]) - 1, std::stod(tokens[2]));
            } else if (tokens[0] == "c") {
                instance.setTaskCost(std::stoi(tokens[2]) - 1, std::stoi(tokens[1]) - 1, std::stoi(tokens[3]));
            }
        }
    }

    for (int i = 0; i < instance.getNumberOfServices(); i++)
        instance.setServResourceCapacity(i, instance.getVres());

    instance.setInstanceName("Instance_10_10_" + std::to_string(instanceCont));
    return instance;
}

static void loadInstanceLog(InstanceMatrix& instance, const string& filename) {
    std::ifstream logFile("data/Log/" + filename);
    if (!logFile.is_open()) return;

    string lastLine, logLine;
    while (std::getline(logFile, logLine)) {
        if (logLine.rfind("Total", 0) == 0) {
            auto eq = logLine.find('=');
            if (eq != string::npos) {
                string rest = logLine.substr(eq + 1);
                auto sec = rest.find("sec");
                if (sec != string::npos) rest = rest.substr(0, sec);
                rest.erase(0, rest.find_first_not_of(" \t"));
                rest.erase(rest.find_last_not_of(" \t") + 1);
                instance.setOptimalExecTime(std::stod(rest));
            }
        }
        lastLine = logLine;
    }
    // Last line: "... = <optimalCost>"
    auto eq = lastLine.rfind("= ");
    if (eq != string::npos)
        instance.setOptimalCost(std::stoi(lastLine.substr(eq + 2)));
}

static void initiateInstanceArray(vector<InstanceMatrix>& instanceArray,
                                  const vector<int>& filter = {}) {
    if (!filter.empty()) {
        for (int i = 0; i < static_cast<int>(filter.size()); i++) {
            int num = filter[i];
            instanceArray[i] = readInstance(num);
            loadInstanceLog(instanceArray[i], "Instance_10_10_" + std::to_string(num));
        }
        return;
    }

    const string folder = "data/instances/";
    vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(folder))
        files.push_back(entry.path());
    std::sort(files.begin(), files.end());

    int cont = 0;
    for (const auto& filePath : files) {
        string filename = filePath.filename().string();

        // Parse instance number from "Instance_10_10_<N>"
        vector<string> parts;
        std::istringstream ss(filename);
        string part;
        while (std::getline(ss, part, '_'))
            parts.push_back(part);

        instanceArray[cont] = readInstance(std::stoi(parts[3]));
        loadInstanceLog(instanceArray[cont], filename);
        cont++;
    }
}

int main() {
    const int executionsPerInstance = 20;

    // To run all instances, leave targetInstances empty: {}
    const vector<int> targetInstances = {11, 28, 100, 128, 129};
    const int numberOfInstances = targetInstances.empty() ? 94
                                                          : static_cast<int>(targetInstances.size());

    int optimalExecTimeDivisor = 10;
    int ITERATIONS = 10000;

    vector<InstanceMatrix> instanceArray(numberOfInstances);
    initiateInstanceArray(instanceArray, targetInstances);

    std::map<int, AlgResult> algResults;
    for (int i = 1; i <= numberOfInstances; i++)
        algResults[i] = AlgResult{};

    int instanceID = 1;

    for (const InstanceMatrix& instance : instanceArray) {
        algResults[instanceID].instanceName = instance.getInstanceName();
        cout << "Executing instance " << instanceID << endl;

        double bestCost   = std::numeric_limits<double>::max();
        double timeToBest = -1;

        const bool hasLogData = instance.getOptimalExecTime() > 0;

        if (hasLogData) {
            if (instance.getOptimalExecTime() < 2) {
                ITERATIONS             = 2000;
                optimalExecTimeDivisor = 20;
            } else {
                ITERATIONS             = 10000;
                optimalExecTimeDivisor = 10;
            }
        } else {
            ITERATIONS             = 10000;
            optimalExecTimeDivisor = 10;
        }

        // Sem log: usa 60s fixos por repetição (replica o comportamento Java)
        double execTimePerRepetition = hasLogData
            ? instance.getOptimalExecTime() / optimalExecTimeDivisor
            : 60.0;

        for (int r = 0; r < executionsPerInstance; r++) {
            cout << "r " << r << endl;

            double instanceInitTime = nowMs();
            Allocation all;

            do {
                ILS ils;
                all = ils.ILSWithRestart(instance, 0.2, ITERATIONS, instanceInitTime,
                    ProbabilityScenario::Ps,
                    ImprovementHeuristic::COST_IMPROVEMENT,
                    SearchMode::LOCAL_SEARCH,
                    ImprovementCondition::BEST_IMPROVEMENT,
                    ImprovementMode::MOVE,
                    PerturbationMode::MOVE);

                if (all.getCurrentCost() < bestCost) {
                    bestCost   = all.getCurrentCost();
                    timeToBest = all.getTimeToBest();
                }

                if (instance.getOptimalCost() > 0 &&
                    all.getCurrentCost() == static_cast<double>(instance.getOptimalCost())) {
                    timeToBest = all.getTimeToBest();
                    break;
                }

            } while ((nowMs() - instanceInitTime) / 1000.0 <= execTimePerRepetition);

            double repetitionExecTime = nowMs() - instanceInitTime;

            timeToBest = (instance.getOptimalCost() > 0 &&
                          bestCost == static_cast<double>(instance.getOptimalCost()))
                ? timeToBest : repetitionExecTime;

            if (algResults[instanceID].bestResult == 0) {
                algResults[instanceID].bestResult = bestCost;
                algResults[instanceID].meanResult = bestCost / executionsPerInstance;
                algResults[instanceID].timeToBest = timeToBest / executionsPerInstance;
            } else {
                algResults[instanceID].meanResult += bestCost / executionsPerInstance;
                algResults[instanceID].timeToBest += timeToBest / executionsPerInstance;
                if (bestCost < algResults[instanceID].bestResult)
                    algResults[instanceID].bestResult = bestCost;
            }

            if (instance.getOptimalCost() > 0 &&
                bestCost == static_cast<double>(instance.getOptimalCost()))
                algResults[instanceID].countBests++;
        }

        instanceID++;
    }

    cout << "Instance | Optimal Cost | Optimal Time | Mean Best Cost | Best Cost | Mean time to Best | Reached Optimal" << endl;
    double instances    = static_cast<double>(algResults.size());
    double meanBestCost = 0;

    int totalReachedOptimal   = 0;
    int totalWithKnownOptimal = 0;
    vector<string> optimalInstances;

    for (auto& [id, result] : algResults) {
        bool hasOptimal     = instanceArray[id - 1].getOptimalCost() > 0;
        bool reachedOptimal = hasOptimal && result.countBests > 0;

        cout << result.instanceName                         << " ";
        cout << instanceArray[id - 1].getOptimalCost()     << " ";
        cout << instanceArray[id - 1].getOptimalExecTime() << " ";
        cout << result.meanResult                          << " ";
        cout << result.bestResult                          << " ";
        meanBestCost += result.bestResult / instances;
        cout << result.timeToBest / 1000.0                 << " ";
        cout << (reachedOptimal
            ? "YES (" + std::to_string((int)result.countBests) + "/" + std::to_string(executionsPerInstance) + ")"
            : (hasOptimal ? "NO" : "N/A")) << endl;

        if (hasOptimal) {
            totalWithKnownOptimal++;
            if (reachedOptimal) {
                totalReachedOptimal++;
                optimalInstances.push_back(result.instanceName);
            }
        }
    }

    cout << "\nMEAN BEST COSTS: " << meanBestCost << endl;
    cout << "REACHED OPTIMALL: " << totalReachedOptimal << "/" << totalWithKnownOptimal << " instances" << endl;

    if (!optimalInstances.empty()) {
        cout << "INSTANCES THAT REACHED OPTIMAL:" << endl;
        for (const auto& name : optimalInstances)
            cout << "  " << name << endl;
    }

    return 0;
}
