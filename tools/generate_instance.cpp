// Gerador de instâncias do problema de seleção de serviços sob incerteza.
//
// Porte do gerador original (gen-inst/main.cpp do projeto de referência) com semente
// explícita e parâmetros configuráveis. Sem opções extras, reproduz as distribuições
// do original, na mesma ordem de sorteio:
//   Vmax ~ U{T/3, T/2}, pmax ~ U(0, 0.4), Vres ~ U{5, 30}, Smax ~ U{T/3, T/2},
//   r_j ~ U{1, 10}, p_s ~ U(0, 0.4), c_sj ~ U{1, 20}.
// Só gera o tipo 2 (uma probabilidade por serviço), o único lido pelos solvers.
//
// Duas opções controlam a dificuldade:
//   --tightness t  fixa Vres para que a soma dos consumos ocupe a fração t da
//                  capacidade de Smax serviços (soma r / (Smax * Vres) ≈ t). As
//                  instâncias com ótimo acima do limite inferior trivial têm t ≈ 0,97.
//   --corr rho     correlaciona custo e confiabilidade: com rho = 1, os serviços com
//                  menor probabilidade de violação são os mais caros.
// Como Vres e os custos são sorteados nas mesmas posições da sequência, a mesma
// semente gera os mesmos consumos e probabilidades para qualquer t e rho.
//
// Uso: generate-instance --seed N --out arquivo [--tasks 100] [--services 100]
//        [--cost-max 20] [--resource-max 10] [--prob-max 0.4]
//        [--tightness t] [--corr rho] [--check-feasible]
// Com --check-feasible, o arquivo só é mantido se a construção inicial da GLS achar
// uma solução viável; caso contrário, é apagado e o programa sai com código 2.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "InitialSolution.hpp"
#include "InstanceReader.hpp"

using std::string;
using std::vector;

struct GeneratorOptions {
    unsigned seed = 0;
    string output;
    int tasks = 100;
    int services = 100;
    int costMax = 20;
    int resourceMax = 10;
    double probMax = 0.4;
    double tightness = 0.0;  // 0 mantém o Vres sorteado, como no original
    double correlation = 0.0;
    bool checkFeasible = false;
};

static GeneratorOptions parseOptions(int argc, char* argv[]) {
    GeneratorOptions options;
    std::map<string, string> values;
    bool hasSeed = false;
    for (int i = 1; i < argc; ++i) {
        const string flag = argv[i];
        if (flag == "--check-feasible") {
            options.checkFeasible = true;
            continue;
        }
        if (flag.rfind("--", 0) != 0 || i + 1 >= argc)
            throw std::invalid_argument("Argumento inválido: " + flag);
        values[flag] = argv[++i];
    }
    for (const auto& [flag, value] : values) {
        if (flag == "--seed") {
            options.seed = static_cast<unsigned>(std::stoul(value));
            hasSeed = true;
        } else if (flag == "--out") options.output = value;
        else if (flag == "--tasks") options.tasks = std::stoi(value);
        else if (flag == "--services") options.services = std::stoi(value);
        else if (flag == "--cost-max") options.costMax = std::stoi(value);
        else if (flag == "--resource-max") options.resourceMax = std::stoi(value);
        else if (flag == "--prob-max") options.probMax = std::stod(value);
        else if (flag == "--tightness") options.tightness = std::stod(value);
        else if (flag == "--corr") options.correlation = std::stod(value);
        else throw std::invalid_argument("Opção desconhecida: " + flag);
    }
    if (!hasSeed || options.output.empty())
        throw std::invalid_argument("--seed e --out são obrigatórios");
    if (options.tasks < 3 || options.services < 1 || options.costMax < 1
            || options.resourceMax < 2 || options.probMax <= 0.0)
        throw std::invalid_argument("Parâmetros de tamanho, custo ou probabilidade inválidos");
    if (options.tightness < 0.0 || options.tightness > 1.0)
        throw std::invalid_argument("--tightness deve estar em (0, 1]");
    if (options.correlation < 0.0 || options.correlation > 1.0)
        throw std::invalid_argument("--corr deve estar em [0, 1]");
    return options;
}

static void writeInstance(const GeneratorOptions& options) {
    const int S = options.services;
    const int T = options.tasks;
    std::mt19937 generator(options.seed);
    std::uniform_int_distribution<int> distVmax(T / 3, T / 2);
    std::uniform_real_distribution<double> distProbViol(0, options.probMax);
    std::uniform_int_distribution<int> distRmax(options.resourceMax / 2, 3 * options.resourceMax);
    std::uniform_int_distribution<int> distSmax(T / 3, static_cast<int>(0.5 * T));

    const int vmax = distVmax(generator);
    const double pmax = distProbViol(generator);
    int vres = distRmax(generator);
    const int smax = distSmax(generator);

    std::uniform_int_distribution<int> distResource(1, options.resourceMax);
    vector<int> resource(T);
    for (int& value : resource)
        value = distResource(generator);

    if (options.tightness > 0.0) {
        const int total = std::accumulate(resource.begin(), resource.end(), 0);
        const int largest = *std::max_element(resource.begin(), resource.end());
        vres = std::max(largest, static_cast<int>(
            std::ceil(total / (options.tightness * smax))));
    }

    std::uniform_real_distribution<double> distProb(0, options.probMax);
    vector<double> prob(S);
    for (double& value : prob)
        value = distProb(generator);

    std::ofstream out(options.output);
    if (!out)
        throw std::runtime_error("Não foi possível criar " + options.output);
    out << 2 << " " << S << " " << T << " " << vmax << " " << pmax << " "
        << smax << " " << vres << "\n";
    for (int j = 0; j < T; ++j)
        out << "r " << j + 1 << " " << resource[j] << "\n";
    for (int s = 0; s < S; ++s)
        out << "p " << s + 1 << " " << prob[s] << "\n";

    std::uniform_int_distribution<int> distCost(1, options.costMax);
    std::uniform_real_distribution<double> distUnit(0.0, 1.0);
    for (int s = 0; s < S; ++s) {
        // Confiabilidade em [0, 1]: 1 para o serviço com probabilidade de violação zero.
        const double reliability = 1.0 - prob[s] / options.probMax;
        for (int j = 0; j < T; ++j) {
            int cost;
            if (options.correlation == 0.0) {
                cost = distCost(generator);
            } else {
                const double mix = (1.0 - options.correlation) * distUnit(generator)
                    + options.correlation * reliability;
                cost = std::min(options.costMax,
                                1 + static_cast<int>(mix * options.costMax));
            }
            out << "c " << s + 1 << " " << j + 1 << " " << cost << "\n";
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        const GeneratorOptions options = parseOptions(argc, argv);
        writeInstance(options);
        if (options.checkFeasible) {
            try {
                buildInitialSolution(readInstanceFile(options.output));
            } catch (const std::runtime_error&) {
                std::filesystem::remove(options.output);
                return 2;
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "[ERRO] " << error.what() << "\n";
        return 1;
    }
    return 0;
}
