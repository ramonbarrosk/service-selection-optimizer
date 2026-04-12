#include <string>

enum SearchMode {
    LOCAL_SEARCH,
    VND
};

inline std::string SearchModeToString(SearchMode mode) {
    switch (mode) {
        case LOCAL_SEARCH: return "Local Search";
        case VND: return "Variable Neighborhood Descent";
        default: return "Unknown Search Mode";
    }
}