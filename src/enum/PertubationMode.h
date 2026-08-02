#pragma once
#include <string>

enum class PerturbationMode {
    MOVE,
    SWAP,
    GRANADE,
    INFEASIBLE_DIVE   // Caramia (2008): mergulha em soluções inviáveis para escapar de ótimos locais
};

inline std::string PerturbationModeToString(PerturbationMode mode) {
    switch (mode) {
        case PerturbationMode::MOVE:           return "Move";
        case PerturbationMode::SWAP:           return "Swap";
        case PerturbationMode::GRANADE:        return "Granade";
        case PerturbationMode::INFEASIBLE_DIVE: return "InfeasibleDive";
        default: return "Unknown Perturbation Mode";
    }
}