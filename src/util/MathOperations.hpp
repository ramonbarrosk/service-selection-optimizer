#pragma once

class MathOperations {
public:
    // C(n, k) calculado de forma incremental com double para evitar overflow.
    static double binominal(int n, int k) {
        if (k < 0 || k > n) return 0.0;
        if (k == 0 || k == n) return 1.0;
        if (k > n - k) k = n - k;

        double result = 1.0;
        for (int i = 1; i <= k; ++i)
            result = result * (n - k + i) / i;
        return result;
    }
};
