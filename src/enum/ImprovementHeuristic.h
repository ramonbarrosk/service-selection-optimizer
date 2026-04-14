#pragma once
#include <string>

using std::string;

enum class ImprovementHeuristic {
    COST_IMPROVEMENT,
    CONSERVATIVE_IMPROVEMENT,
    BALANCED_IMPROVEMENT
};

inline string improvementHeuristicToString(ImprovementHeuristic heuristic) {
    switch (heuristic) {
        case ImprovementHeuristic::COST_IMPROVEMENT: return "Cost Improvement";
        case ImprovementHeuristic::CONSERVATIVE_IMPROVEMENT: return "Conservative Improvement";
        case ImprovementHeuristic::BALANCED_IMPROVEMENT: return "Balanced Improvement";
        default: return "Unknown Improvement Heuristic";
    }
}