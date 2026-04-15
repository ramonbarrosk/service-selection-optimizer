#pragma once

class MathOperations {
public:
    static double binominal(int n, int k) {
        return factorial(n) / (factorial(k) * factorial(n - k));
    }

private:
    static int factorial(int n) {
        int result = 1;
        for (int i = 2; i <= n; ++i)
            result *= i;
        return result;
    }
};
