
#include "SolutionValidator.h"
#include <cmath>
#include <algorithm>

using std::vector;


bool SolutionValidator::isFeasible(Allocation& all, InstanceMatrix& instance,
                                   int Vmax, int Smax, double Pmax,
                                   ProbabilityScenario pScenario, bool verifyProbRestriction) {


    bool probRestriction = false;
    // Verificar restrição de número de serviços empregados
    if (all.getNumberOfEmployedServices() > Smax)
        return false;

    // Verificar restrição de consumo de recursos
    if (!all.respectsResourceRestriction(instance))
        return false;

    // Verificar restrição de probabilidade
    if (pScenario == ProbabilityScenario::P) {
        double probSum = 0.0;

        int numberOfTasks = all.getAllocation().size();

        for (int k = 0; k <= Vmax; ++k) {
            double p = instance.getProbabilityPerService()[0];
            double binom = MathOperations::binominal(numberOfTasks, k);
            probSum += binom * pow(Pmax, k) * pow(1 - Pmax, numberOfTasks - k);
        }
    }
    

    return true; // Solução é factível
}