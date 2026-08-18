#pragma once
#include <string>

using std::string;

// Tipo de vizinhança: MOVE transfere uma tarefa; SWAP troca duas tarefas.
enum class ImprovementMode {
    MOVE,
    SWAP
};

inline string ImprovementModeToString(ImprovementMode heuristic) {
    switch (heuristic) {
        case ImprovementMode::MOVE: return "Move";
        case ImprovementMode::SWAP: return "Swap";
        default: return "Unknown Improvement Mode";
    }
}
