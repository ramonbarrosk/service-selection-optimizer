#pragma once
#include <string>

using std::string;

// Define se a busca aceita a primeira melhoria encontrada ou examina toda a
// vizinhança para escolher a melhor melhoria da rodada.
enum class ImprovementCondition {
    FIRST_IMPROVEMENT,
    BEST_IMPROVEMENT
};

inline string improvementConditionToString(ImprovementCondition condition) {
    switch (condition) {
        case ImprovementCondition::FIRST_IMPROVEMENT: return "First Improvement";
        case ImprovementCondition::BEST_IMPROVEMENT: return "Best Improvement";
        default: return "Unknown Improvement Condition";
    }
}
