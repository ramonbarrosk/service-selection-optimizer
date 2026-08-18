#pragma once
#include <string>

// Movimento usado para afastar o ILS do mínimo local atual.
enum class PerturbationMode {
    MOVE,
    SWAP,
    GRANADE
};

inline std::string PerturbationModeToString(PerturbationMode mode) {
    switch (mode) {
        case PerturbationMode::MOVE:           return "Move";
        case PerturbationMode::SWAP:           return "Swap";
        case PerturbationMode::GRANADE: return "Granade";
        default: return "Unknown Perturbation Mode";
    }
}
