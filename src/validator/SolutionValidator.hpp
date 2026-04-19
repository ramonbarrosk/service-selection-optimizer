
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
    bool isFeasible(const InstanceMatrix& instance, Allocation& allocation,
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
                                // i = número de tarefas consideradas, k = número de violações de SLA
                                vector<vector<double>> dinamicPrograming(numberOfTasks + 1, std::vector<double>(vMax + 1, 0.0));


                                for (int i = 0; i <= numberOfTasks; ++i) {
                                    double sumProb = 0.0;
                                    double probi_1 = 0.0;

                                    if (i > 0) {
                                        Task task(i - 1, instance.getTaskConsumption(i - 1));
                                        auto it = allocation.getAllocation().find(task.getTaskId());
                                        if (it != allocation.getAllocation().end()) {
                                            probi_1 = probabilities[it->second];
                                        }

                                    }

                                    for (int k = 0; k <= vMax; ++k) {
                                        if (i == 0 && k == 0) {
                                            dinamicPrograming[i][k] = 1.0; // Probabilidade de 0 tarefas violarem o SLA é 1
                                        } else if (i == 0) {
                                            dinamicPrograming[i][k] = 0.0; // Probabilidade de mais de 0 tarefas violarem o SLA é 0
                                        } else if (k == 0) {
                                            dinamicPrograming[i][k] = dinamicPrograming[i - 1][k] * (1 - probi_1); // Nenhuma tarefa viola o SLA
                                        } else {
                                            dinamicPrograming[i][k] = dinamicPrograming[i - 1][k] * (1 - probi_1) + 
                                                                     dinamicPrograming[i - 1][k - 1] * probi_1; // k tarefas violam o SLA
                                        }

                                        sumProb += dinamicPrograming[i][k];
                                    }

                                    if (1.0 - sumProb > Pmax) {
                                        return false; // Probabilidade de violação do SLA é maior que Pmax
                                    }
                                }

                                double pNotViolateSLA = 0.0; // Probabilidade de nenhuma tarefa violar o SLA

                                for (int k = 0; k <= Vmax; ++k) {
                                    pNotViolateSLA += dinamicPrograming[numberOfTasks][k];
                                }

                                if (1.0 - pNotViolateSLA <= Pmax) {
                                    allocation.setCurrentProb(1.0 - pNotViolateSLA);
                                    return true; //
                                } else {
                                    return false; // Probabilidade de violação do SLA é maior que Pmax
                                }
                            } else {
                                return true; // Se não for necessário verificar a restrição de probabilidade, consideramos a solução como viável
                            }
                        }
                        
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
        
};



