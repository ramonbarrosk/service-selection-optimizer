#include <string>

enum ImprovementHeuristic {
    COST_IMPROVEMENT,
    CONSERVATIVE_IMPROVEMENT,
    BALANCED_IMPROVEMENT
};

inline std::string improvementHeuristicToString(ImprovementHeuristic heuristic) {
    switch (heuristic) {
        case COST_IMPROVEMENT: return "Cost Improvement";
        case CONSERVATIVE_IMPROVEMENT: return "Conservative Improvement";
        case BALANCED_IMPROVEMENT: return "Balanced Improvement";
        default: return "Unknown Improvement Heuristic";
    }
}