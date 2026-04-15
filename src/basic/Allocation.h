#pragma once

#include <unordered_map>
#include <vector>
#include "Task.h"
#include "Service.h"
#include "InstanceMatrix.hpp"

using std::vector;
using std::unordered_map;

// Stub mínimo de InstanceMatrix — será expandido quando o módulo instance for criado

class Allocation {
public:
    Allocation()
        : currentCost_(0.0), currentProb_(0.0), timeToBest_(0.0) {}

    Allocation(const Allocation& other) = default;
    Allocation& operator=(const Allocation& other) = default;

    int numberOfTasksAllocated() const {
        return static_cast<int>(allocation_.size());
    }

    void addTask(const Task& t, const Service& s, const InstanceMatrix& instance) {
        int taskId = t.getTaskId();
        int servId = s.getServId();

        currentCost_ += instance.getTaskCost(taskId, servId);

        if (allocation_.find(taskId) == allocation_.end())
            allocation_[taskId] = servId;

        employedServices_[servId]++;
        resourcePerService_[servId] += t.getResourceConsumption();
    }

    void replaceService(const Task& t, const Service& newS, const InstanceMatrix& instance) {
        int taskId   = t.getTaskId();
        int oldServId = allocation_[taskId];
        int newServId = newS.getServId();

        currentCost_ -= instance.getTaskCost(taskId, oldServId);

        if (--employedServices_[oldServId] == 0)
            employedServices_.erase(oldServId);

        resourcePerService_[oldServId] -= t.getResourceConsumption();

        allocation_[taskId] = newServId;

        currentCost_ += instance.getTaskCost(taskId, newServId);

        employedServices_[newServId]++;
        resourcePerService_[newServId] += t.getResourceConsumption();
    }

    void removeTask(const Task& t, const InstanceMatrix& instance) {
        int taskId = t.getTaskId();
        int servId = allocation_[taskId];

        allocation_.erase(taskId);
        currentCost_ -= instance.getTaskCost(taskId, servId);

        if (--employedServices_[servId] == 0)
            employedServices_.erase(servId);

        resourcePerService_[servId] -= t.getResourceConsumption();
    }

    bool respectsResourceRestriction(const InstanceMatrix& instance) const {
        for (const auto& [servId, consumption] : resourcePerService_)
            if (consumption > instance.getVres())
                return false;
        return true;
    }

    int getNumberOfEmployedServices() const {
        return static_cast<int>(employedServices_.size());
    }

    int getServiceForTask(int taskId) const {
        return allocation_.at(taskId);
    }

    const unordered_map<int, int>& getAllocation() const { return allocation_; }
    void setAllocation(const unordered_map<int, int>& allocation) { allocation_ = allocation; }

    double getCurrentCost() const { return currentCost_; }
    void setCurrentCost(double cost) { currentCost_ = cost; }

    double getCurrentProb() const { return currentProb_; }
    void setCurrentProb(double prob) { currentProb_ = prob; }

    double getTimeToBest() const { return timeToBest_; }
    void setTimeToBest(double time) { timeToBest_ = time; }

private:
    // taskId -> serviceId
    unordered_map<int, int> allocation_;
    // serviceId -> total resource consumption
    unordered_map<int, int> resourcePerService_;
    // serviceId -> number of allocated tasks
    unordered_map<int, int> employedServices_;

    double currentCost_;
    double currentProb_;
    double timeToBest_;
};
