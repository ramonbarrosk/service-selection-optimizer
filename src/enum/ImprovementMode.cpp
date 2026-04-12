#include <string>

enum ImprovementMode {
    MOVE,
    SWAP
};

inline std::string ImprovementModeToString(ImprovementMode heuristic) {
    switch (heuristic) {
        case MOVE: return "Move";
        case SWAP: return "Swap";
        default: return "Unknown Improvement Mode";
    }
}