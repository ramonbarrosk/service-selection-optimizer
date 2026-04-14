#pragma once
#include <string>

using std::string;

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