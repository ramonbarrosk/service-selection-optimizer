#include <string>

enum PerturbationMode {
    MOVE,
    SWAP,
    GRANADE
};

inline std::string PerturbationModeToString(PerturbationMode mode) {
    switch (mode) {
        case MOVE: return "Move";
        case SWAP: return "Swap";
        case GRANADE: return "Granade";
        default: return "Unknown Perturbation Mode";
    }
}