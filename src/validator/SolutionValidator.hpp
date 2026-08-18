
#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include "InstanceMatrix.hpp"
#include "../util/MathOperations.hpp"
#include <ProbabilityScenario.h>
#include <Task.h>
#include <Service.h>
#include <Allocation.h>

using std::vector;


class SolutionValidator {
public:
    // Verifica as restrições duras nesta ordem: número de serviços, capacidade
    // e SLA probabilística. As duas primeiras são baratas e eliminam candidatos
    // antes do cálculo mais custoso da distribuição de probabilidade.
    bool isFeasible(const InstanceMatrix& instance, Allocation& allocation,
                    int Vmax, int Smax, double Pmax, ProbabilityScenario& scenario,
                    bool verifyProbRestriction) {
        if (allocation.getNumberOfEmployedServices() > Smax)
            return false;

        if (!allocation.respectsResourceRestriction(instance))
            return false;

        if (scenario == ProbabilityScenario::P) {
            double probSum = 0.0;
            int numberOfTasks = allocation.numberOfTasksAllocated();
            double p = instance.getProbabilityPerService()[0];

            for (int k = 0; k <= Vmax; ++k) {
                double binom = MathOperations::binominal(numberOfTasks, k);
                probSum += binom * pow(p, k) * pow(1 - p, numberOfTasks - k);
            }
            return true;
        }

        if (scenario == ProbabilityScenario::Ps) {
            // Quando o movimento não pode piorar a SLA, o chamador pode evitar
            // o cálculo dinâmico informando verifyProbRestriction=false.
            if (!verifyProbRestriction)
                return true;

            int vMax = instance.getVmax();
            int numberOfTasks = instance.getNumberOfTasks();
            const vector<double>& probabilities = instance.getProbabilityPerService();
            const vector<int>& alloc = allocation.getAllocation();

            // A DP calcula a distribuição Poisson-binomial do número de violações.
            // O buffer achatado é reutilizado para evitar alocações repetidas no heap.
            int cols = vMax + 1;
            int needed = (numberOfTasks + 1) * cols;
            if ((int)dp_.size() < needed) dp_.assign(needed, 0.0);
            auto cell = [&](int i, int k) -> double& { return dp_[i * cols + k]; };

            for (int i = 0; i <= numberOfTasks; ++i) {
                double sumProb = 0.0;
                double probi_1 = 0.0;

                if (i > 0 && (i - 1) < (int)alloc.size() && alloc[i - 1] >= 0)
                    probi_1 = probabilities[alloc[i - 1]];

                for (int k = 0; k <= vMax; ++k) {
                    if (i == 0 && k == 0)
                        cell(i, k) = 1.0;
                    else if (k > i)
                        cell(i, k) = 0.0;
                    else if (k == 0)
                        cell(i, k) = cell(i - 1, k) * (1 - probi_1);
                    else
                        cell(i, k) = cell(i - 1, k) * (1 - probi_1) + cell(i - 1, k - 1) * probi_1;

                    sumProb += cell(i, k);
                }

                if (1.0 - sumProb > Pmax)
                    return false;
            }

            double pNotViolateSLA = 0.0;
            for (int k = 0; k <= Vmax; ++k)
                pNotViolateSLA += cell(numberOfTasks, k);

            if (1.0 - pNotViolateSLA <= Pmax) {
                allocation.setCurrentProb(1.0 - pNotViolateSLA);
                return true;
            }
            return false;
        }

        return false;
    }

    // Retorna quanto a probabilidade de violação excede Pmax (0 se viável).
    // Roda o DP completo sem early-exit para medir o grau de inviabilidade.
    double computeViolationExcess(const InstanceMatrix& instance, const Allocation& allocation,
                                  int Vmax, double Pmax, ProbabilityScenario& scenario) {
        if (scenario != ProbabilityScenario::Ps)
            return 0.0;

        int numberOfTasks = instance.getNumberOfTasks();
        const vector<double>& probabilities = instance.getProbabilityPerService();
        const vector<int>& alloc = allocation.getAllocation();

        int cols = Vmax + 1;
        int needed = (numberOfTasks + 1) * cols;
        if ((int)dp_.size() < needed) dp_.assign(needed, 0.0);
        auto cell = [&](int i, int k) -> double& { return dp_[i * cols + k]; };

        for (int i = 0; i <= numberOfTasks; ++i) {
            double probi_1 = 0.0;
            if (i > 0 && (i - 1) < (int)alloc.size() && alloc[i - 1] >= 0)
                probi_1 = probabilities[alloc[i - 1]];
            for (int k = 0; k <= Vmax; ++k) {
                if (i == 0 && k == 0)       cell(i, k) = 1.0;
                else if (k > i)             cell(i, k) = 0.0;
                else if (k == 0)            cell(i, k) = cell(i-1, k) * (1 - probi_1);
                else                        cell(i, k) = cell(i-1, k) * (1 - probi_1) + cell(i-1, k-1) * probi_1;
            }
        }

        double pNotViolateSLA = 0.0;
        for (int k = 0; k <= Vmax; ++k)
            pNotViolateSLA += cell(numberOfTasks, k);

        return std::max(0.0, (1.0 - pNotViolateSLA) - Pmax);
    }

    vector<int> removeEqual(const std::vector<int>& a_arr, const std::vector<int>& b_arr) {
        std::vector<int> result;
        for (int a : a_arr) {
            bool found = false;
            for (int b : b_arr) {
                if (a == b) { found = true; break; }
            }
            if (!found) result.push_back(a);
        }
        return result;
    }

private:
    vector<double> dp_;
};
