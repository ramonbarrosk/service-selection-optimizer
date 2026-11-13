// Uma execução da GLS em um arquivo de instância, para a calibração com o irace.
//
// Uso: gls-run <arquivo da instância> <segundos>
// Os parâmetros da GLS vêm das variáveis SSO_GLS_* (glsParametersFromEnv). A execução
// usa o orçamento inteiro: sem log do método exato, não há parada antecipada.
// Imprime em stdout o GAP percentual para o limite inferior trivial, o valor que o
// irace minimiza; o limite normaliza a escala de custo entre instâncias. O custo vai
// para stderr.

#include <iostream>
#include <stdexcept>
#include <string>

#include "GlsSolver.hpp"
#include "InitialSolution.hpp"
#include "InstanceReader.hpp"

int main(int argc, char* argv[]) {
    try {
        if (argc != 3)
            throw std::invalid_argument("uso: gls-run <instância> <segundos>");
        const GlsParameters parameters = glsParametersFromEnv();
        const double budgetSeconds = std::stod(argv[2]);
        const InstanceMatrix instance = readInstanceFile(argv[1]);
        const int bound = trivialLowerBound(instance);
        const GlsRunResult run = runGuidedLocalSearch(
            buildInitialSolution(instance), instance, budgetSeconds, parameters.options,
            parameters.alpha, parameters.roundsPerCycle);
        const double cost = run.best.getCurrentCost();
        std::cerr << "cost=" << cost << " lower_bound=" << bound << "\n";
        std::cout << 100.0 * (cost - bound) / bound << "\n";
    } catch (const std::exception& error) {
        std::cerr << "[ERRO] " << error.what() << "\n";
        return 1;
    }
    return 0;
}
