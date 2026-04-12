#include <string>

enum ImprovementCondition {
    FIRST_IMPROVEMENT,
    BEST_IMPROVEMENT
};

inline std::string improvementConditionToString(ImprovementCondition condition) {
    switch (condition) {
        case FIRST_IMPROVEMENT: return "First Improvement";
        case BEST_IMPROVEMENT: return "Best Improvement";
        default: return "Unknown Improvement Condition";
    }
}