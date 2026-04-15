
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
    bool isFeasible(const InstanceMatrix& instance, const Allocation& allocation,
                    int Vmax, int Smax, double Pmax, ProbabilityScenario& scenario, 
                    bool verifyProbRestriction) {
                        if (allocation.getNumberOfEmployedServices() > Smax)
                            return false;
                        
                        if (!allocation.respectsResourceRestriction(instance))
                            return false;

                        if (scenario == ProbabilityScenario::P) {
                            double probSum = 0.0;
                            int numberOfTasks = (int)allocation.getAllocation().size();
                            double p = instance.getProbabilityPerService()[0];

                            for (int k = 0; k <= Vmax; ++k) {
                                double binom = MathOperations::binominal(numberOfTasks, k);
                                probSum += binom * pow(p, k) * pow(1 - p, numberOfTasks - k);
                            }
                        } else if (scenario == ProbabilityScenario::Ps) {
                            if (verifyProbRestriction) {
                                int vMax = instance.getVmax();
                                int numberOfTasks = instance.getNumberOfTasks();
                                const std::vector<double>& probabilities = instance.getProbabilityPerService();

                                //Matriz de programção dinâmica para armazenar as probabilidades acumuladas
                                vector<vector<double>> dinamicPrograming(numberOfTasks + 1, std::vector<double>(vMax + 1, 0.0));

                                // IMPORTANTE: CONTRUIR A LÓGICA DE PROGRAMAÇÃO DINÂMICA PARA CALCULAR A PROBABILIDADE DE VIOLAÇÃO DE SLA
                            }
                        }
                        
    }


    // IMPORTANTE: REVISAR ESSA LÓGICA
    vector<int> removeEqual(vector<int>& vec, vector<int>& vec2) {
        vector<int> result;
        for (int num : vec) {
            if (std::find(vec2.begin(), vec2.end(), num) == vec2.end()) {
                result.push_back(num);
            }
        }
        return result;
    }
        
};



