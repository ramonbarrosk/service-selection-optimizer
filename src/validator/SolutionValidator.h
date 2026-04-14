
#pragma once
#include "Allocation.h"
#include "Service.h"
#include "ProbabilityScenario.h"


using std::vector;


class SolutionValidator {
public:
    bool isFeasible(Allocation& all, InstanceMatrix& instance,
                    int Vmax, int Smax, double Pmax,
                    ProbabilityScenario pScenario, bool verifyProbRestriction);

    static vector<int> removeEqual(const vector<int>& a_arr, const vector<int>& b_arr);
};