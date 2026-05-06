#pragma once
#include <random>
#include "Service.h"
#include <vector>
using std::random_device;
using std::mt19937;
using std::uniform_int_distribution;
using std::vector;

class RandomUtil {
public:
    static int getRandomInt(int min, int max) {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dis(min, max);
        return dis(gen);
    }

    static Service getRandomService(int numberofServices) {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dis(0, numberofServices - 1);
        return Service(dis(gen));
    }

    static int getRandomNumber(int number) {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dis(0, number - 1);
        return dis(gen);
    }

    static int getRandomTaskId(int numberOfTasks) {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dis(0, numberOfTasks - 1);
        return dis(gen);
    }

    static Service getRandomService(const vector<Service>& services) {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dis(0, static_cast<int>(services.size()) - 1);
        return services[dis(gen)];
    }

    static int getRandomTaskIdFromList(const vector<int>& taskIds) {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dis(0, static_cast<int>(taskIds.size()) - 1);
        return taskIds[dis(gen)];
    }
};