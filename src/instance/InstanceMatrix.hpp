#pragma once

#include <vector>
#include <string>
#include <limits>
#include <stdexcept>
#include <Service.h>

using std::vector;


class InstanceMatrix {
public:
    InstanceMatrix() : 
        vRes(0), vMax(0), Smax(0), pMax(0.0),
        optimalCost(0), optimalExecTime(0.0),
        numberOfServices(0), numberOfTasks(0) {}

    InstanceMatrix(int numberOfTasks, int numberOfServices, int vMax, int Smax, double pMax, int Vres)
        : costMatrix(numberOfTasks, vector<int>(numberOfServices, 0)),
          vRes(Vres), vMax(vMax), Smax(Smax), pMax(pMax),
          optimalCost(0), optimalExecTime(0.0),
          numberOfServices(numberOfServices), numberOfTasks(numberOfTasks),
          probabilityPerService(numberOfServices, 0.0),
          servResourceCapacity(numberOfServices, 0),
          taskResourceConsumption(numberOfTasks, 0) {}
    
    

    int getVres() const { return vRes; }
    void setVres(int vRes) { vRes = vRes; }

    const string& getInstanceName() const { return instanceName_; }
    void setInstanceName(const string& name) { instanceName_ = name; }

    double getOptimalExecTime() const { return optimalExecTime; }
    void setOptimalExecTime(double time) { optimalExecTime = time; }

    int getOptimalCost() const { return optimalCost; }
    void setOptimalCost(int cost) { optimalCost = cost; }

    int getVmax() const { return vMax; }
    void setVmax(int vMax) { this->vMax = vMax; }

    int getSmax() const { return Smax; }
    void setSmax(int sMax) { Smax = sMax; }

    double getPmax() const { return pMax; }
    void setPmax(double pMax) { this->pMax = pMax; }

    int getNumberOfServices() const { return numberOfServices; }
    void setNumberOfServices(int num) { numberOfServices = num; }

    int getNumberOfTasks() const { return numberOfTasks; }
    void setNumberOfTasks(int num) { numberOfTasks = num; }

    // -- Probability -- 

    double getServiceProb(int serviceId) const {
        return probabilityPerService.at(serviceId);
    }

    double getTaskProb(int taskId, int serviceId) const {
        return probabilityPerService.at(serviceId);
    }

    const vector<double>& getProbabilityPerService() const { return probabilityPerService; }

    void setProbMatrix(const vector<double>& probabilities) {
        probabilityPerService = probabilities;
    }

    void setSlaViolationProbability(int serviceId, double probability) {
        probabilityPerService[serviceId] = probability;
    }

    // -- Resource Capacity -- 

    const vector<int>& getServResourceCapacity() const { return servResourceCapacity; }
    void setServiceFullResourceCapacity(const vector<int>& capacities) { servResourceCapacity = capacities; }

    void setServResourceCapacity(int serviceId, int capacity) {
        servResourceCapacity[serviceId] = capacity;
    }

    // -- Task Consumption --

    const vector<int>& getTaskConsumption() const { return taskResourceConsumption; }

    int getTaskConsumption(int taskId) const {
        return taskResourceConsumption.at(taskId);
    }

    void setTaskConsumption(const vector<int>& consumption) {
        taskResourceConsumption = consumption;
    }

    void setResourceConsumption(int taskId, int consumption) {
        taskResourceConsumption[taskId] = consumption;
    }

    // -- Cost Matrix --

    void setTaskCost(int taskId, int serviceId, int taskCost) {
        costMatrix[taskId][serviceId] = taskCost;
    }

    int getTaskCost(int taskId, int serviceId) const {
        return costMatrix.at(taskId).at(serviceId);
    }

    // -- Cost Queries --

    double getMinCostForTask(int taskId) const {
       double minCost = std::numeric_limits<double>::max();
        for (int cost : costMatrix.at(taskId)) {
           if (cost < minCost) {
               minCost = cost;
           }
        }
        return minCost;
    }

    double getMaxCostForTask(int taskId) const {
       double maxCost = std::numeric_limits<double>::min();
        for (int cost : costMatrix.at(taskId)) {
           if (cost > maxCost) {
               maxCost = cost;
           }
        }
        return maxCost;
    }


    //  "Quais serviços posso usar para essa tarefa sem ultrapassar o custo máximo?"

    vector<Service> getServicesWithMaxCost(int taskId, double maxCost) const {
        vector<Service> services;
        for (int i = 0; i < (int)costMatrix.at(taskId).size(); ++i) {
            if (costMatrix.at(taskId).at(i) <= maxCost)
            services.emplace_back(i, costMatrix.at(taskId).at(i));
        }
        return services;
    }

    //  "Quais serviço com custo mínimo para essa tarefa?"

    Service getServiceWithLowestCost(int taskId) const {
        int minCost = std::numeric_limits<int>::max();
        int bestServiceId = -1;
        for (int i = 0; i < (int)costMatrix.at(taskId).size(); ++i) {
            if (costMatrix.at(taskId).at(i) <= minCost) {
                minCost = costMatrix.at(taskId).at(i);
                bestServiceId = i;
            }
        }
        return Service(bestServiceId, minCost);
    }

    // "Quais serviço com menor probabilidade de violação de SLA para essa tarefa?"

    Service getServiceWithLowestProbability(int taskId) const {
        double minProb = std::numeric_limits<double>::max();
        int bestServiceId = -1;
        for (int i = 0; i < (int)probabilityPerService.size(); ++i) {
            if (probabilityPerService.at(i) <= minProb) {
                minProb = probabilityPerService.at(i);
                bestServiceId = i;
            }
        }
        return Service(bestServiceId, minProb);
    }




private:
    vector<vector<int>> costMatrix;

    string instanceName_;

    int vRes;

    int vMax;
    int Smax;
    double pMax;

    int optimalCost;
    double optimalExecTime;

    vector<double> probabilityPerService;
    vector<int> servResourceConsumption;
    vector<int> servResourceCapacity;
    vector<int> taskResourceConsumption;

    int numberOfTasks;
    int numberOfServices;
};