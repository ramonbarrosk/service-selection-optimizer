#pragma once
#include <string>

enum class SearchMode {
    LOCAL_SEARCH,
    VND
};

inline std::string SearchModeToString(SearchMode mode) {
    switch (mode) {
        case SearchMode::LOCAL_SEARCH: return "Local Search";
        case SearchMode::VND: return "Variable Neighborhood Descent";
        default: return "Unknown Search Mode";
    }
}