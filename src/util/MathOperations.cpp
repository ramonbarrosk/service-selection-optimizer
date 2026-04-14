class MathOperations {
    static double binominal(int i, int k) {
        return factorial(i) / (factorial(k) * factorial(i - k));
    }

    static int factorial(int n) {
        int factorial = 1;
        for (int i = 2; i <= n; ++i)
            factorial *= i;
        return factorial;
    }
};