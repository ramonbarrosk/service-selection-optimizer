#pragma once

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "InstanceMatrix.hpp"

// Leitura das instâncias e dos logs do método exato. Por padrão, data/instances
// e data/Log, relativos à raiz do repositório; SSO_INSTANCE_DIR e SSO_LOG_DIR
// apontam para outro conjunto, como as instâncias geradas em data/instances_hard.

inline const std::string kInstancePrefix = "Instance_10_10_";

inline std::string envDir(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return value && *value ? std::string(value) : fallback;
}

inline std::string instanceDir() { return envDir("SSO_INSTANCE_DIR", "data/instances"); }
inline std::string logDir() { return envDir("SSO_LOG_DIR", "data/Log"); }

inline std::vector<int> listInstanceIds(const std::string& folder = instanceDir()) {
    std::vector<int> ids;
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (!entry.is_regular_file())
            continue;
        const std::string name = entry.path().filename().string();
        if (name.rfind(kInstancePrefix, 0) == 0)
            ids.push_back(std::stoi(name.substr(kInstancePrefix.size())));
    }
    std::sort(ids.begin(), ids.end());
    if (ids.empty())
        throw std::runtime_error("Nenhuma instância encontrada em " + folder);
    return ids;
}

inline InstanceMatrix readInstanceFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Não foi possível abrir: " + path);

    bool firstLine = true;
    InstanceMatrix instance;
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream input(line);
        std::vector<std::string> tokens;
        std::string token;
        while (input >> token)
            tokens.push_back(token);
        if (tokens.empty())
            continue;

        // Cabeçalho: <nome> serviços tarefas Vmax Pmax Smax Vres.
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
    instance.setInstanceName(std::filesystem::path(path).filename().string());
    return instance;
}

inline InstanceMatrix readInstance(int id) {
    return readInstanceFile(instanceDir() + "/" + kInstancePrefix + std::to_string(id));
}

// Limite inferior trivial: cada tarefa no seu serviço mais barato, ignorando
// capacidade, Smax e SLA. Uma solução viável com esse custo é ótima.
inline int trivialLowerBound(const InstanceMatrix& instance) {
    int bound = 0;
    for (int task = 0; task < instance.getNumberOfTasks(); ++task)
        bound += static_cast<int>(instance.getMinCostForTask(task));
    return bound;
}

// Carrega o custo ótimo e o tempo do método exato. Retorna false se o log não
// existir ou não trouxer o ótimo; nesse caso a instância fica sem referência.
inline bool loadReferenceLog(InstanceMatrix& instance, int id) {
    std::ifstream file(logDir() + "/" + kInstancePrefix + std::to_string(id));
    if (!file.is_open())
        return false;

    std::string line;
    std::string lastLine;
    while (std::getline(file, line)) {
        if (line.rfind("Total", 0) == 0) {
            const auto equal = line.find('=');
            const auto seconds = line.find("sec");
            if (equal != std::string::npos) {
                instance.setOptimalExecTime(std::stod(line.substr(
                    equal + 1,
                    seconds == std::string::npos
                        ? std::string::npos : seconds - equal - 1)));
            }
        }
        if (!line.empty())
            lastLine = line;
    }
    // A última linha do log tem o formato "... = <custo ótimo>".
    const auto equal = lastLine.rfind("= ");
    if (equal == std::string::npos)
        return false;
    instance.setOptimalCost(std::stoi(lastLine.substr(equal + 2)));
    return true;
}
