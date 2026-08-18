#pragma once
#include <random>
#include "Service.h"
#include <vector>
using std::mt19937;
using std::uniform_int_distribution;
using std::vector;

class RandomUtil {
public:
    static mt19937& engine() {
        thread_local mt19937 gen(std::random_device{}());
        return gen;
    }

    // Redefine a semente do gerador local ao fluxo de execução. Isso permite repetir um
    // experimento ou comparar duas configurações usando as mesmas escolhas aleatórias.
    static void setSeed(unsigned s) { engine().seed(s); }

    static int getRandomInt(int min, int max) {
        uniform_int_distribution<> dis(min, max);
        return dis(engine());
    }

    static Service getRandomService(int numberofServices) {
        uniform_int_distribution<> dis(0, numberofServices - 1);
        return Service(dis(engine()));
    }

    static int getRandomNumber(int number) {
        uniform_int_distribution<> dis(0, number - 1);
        return dis(engine());
    }

    static int getRandomTaskId(int numberOfTasks) {
        uniform_int_distribution<> dis(0, numberOfTasks - 1);
        return dis(engine());
    }

    static Service getRandomService(const vector<Service>& services) {
        uniform_int_distribution<> dis(0, static_cast<int>(services.size()) - 1);
        return services[dis(engine())];
    }

    static int getRandomTaskIdFromList(const vector<int>& taskIds) {
        uniform_int_distribution<> dis(0, static_cast<int>(taskIds.size()) - 1);
        return taskIds[dis(engine())];
    }
};
