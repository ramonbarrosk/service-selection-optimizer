#pragma once
#include <string>

using std::string;

enum class ProbabilityScenario {
    P,
    Ps,
    Pts
};

inline string ProbabilityScenarioToString(ProbabilityScenario scenario) {
    switch (scenario) {
        case ProbabilityScenario::P:   return "P";
        case ProbabilityScenario::Ps:  return "Ps";
        case ProbabilityScenario::Pts: return "Pts";
        default:                       return "Unknown Probability Scenario";
    }
}