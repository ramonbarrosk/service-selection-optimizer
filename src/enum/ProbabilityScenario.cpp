#include <string>

enum ProbabilityScenario {
    P,
    Ps,
    Pts
};

inline std::string ProbabilityScenarioToString(ProbabilityScenario scenario) {
    switch (scenario) {
        case P: return "P";
        case Ps: return "Ps";
        case Pts: return "Pts";
        default: return "Unknown Probability Scenario";
    }
}