#pragma once

#include <vector>
#include "Task.h"
#include "Service.h"
#include "InstanceMatrix.hpp"

using std::vector;

// Representa uma solução: para cada tarefa, guarda o serviço escolhido e mantém
// incrementalmente o custo, o consumo por serviço e os contadores necessários.
class Allocation {
public:
    Allocation()
        : currentCost_(0.0), currentProb_(0.0), timeToBest_(0.0),
          numberOfTasksAllocated_(0), numberOfEmployedServices_(0) {}

    Allocation(const Allocation& other) = default;
    Allocation& operator=(const Allocation& other) = default;
    Allocation(Allocation&&) noexcept = default;
    Allocation& operator=(Allocation&&) noexcept = default;

    int numberOfTasksAllocated() const { return numberOfTasksAllocated_; }

    bool hasTask(int taskId) const {
        return taskId >= 0 && taskId < (int)allocation_.size() && allocation_[taskId] >= 0;
    }

    void addTask(const Task& t, const Service& s, const InstanceMatrix& instance) {
        // Mantém custo, ocupação e quantidade de serviços atualizados de forma
        // incremental, evitando recalcular toda a solução após cada movimento.
        ensureSized(instance);
        int taskId = t.getTaskId();
        int servId = s.getServId();

        if (allocation_[taskId] >= 0) return; // já alocado

        currentCost_ += instance.getTaskCost(taskId, servId);
        allocation_[taskId] = servId;
        numberOfTasksAllocated_++;

        if (employedServices_[servId]++ == 0)
            numberOfEmployedServices_++;

        resourcePerService_[servId] += t.getResourceConsumption();
    }

    void replaceService(const Task& t, const Service& newS, const InstanceMatrix& instance) {
        // Remove a contribuição do serviço antigo e acrescenta a do novo.
        int taskId = t.getTaskId();
        int oldServId = allocation_[taskId];
        int newServId = newS.getServId();

        currentCost_ -= instance.getTaskCost(taskId, oldServId);

        if (--employedServices_[oldServId] == 0)
            numberOfEmployedServices_--;

        resourcePerService_[oldServId] -= t.getResourceConsumption();

        allocation_[taskId] = newServId;

        currentCost_ += instance.getTaskCost(taskId, newServId);

        if (employedServices_[newServId]++ == 0)
            numberOfEmployedServices_++;

        resourcePerService_[newServId] += t.getResourceConsumption();
    }

    void removeTask(const Task& t, const InstanceMatrix& instance) {
        int taskId = t.getTaskId();
        int servId = allocation_[taskId];

        allocation_[taskId] = -1;
        numberOfTasksAllocated_--;
        currentCost_ -= instance.getTaskCost(taskId, servId);

        if (--employedServices_[servId] == 0)
            numberOfEmployedServices_--;

        resourcePerService_[servId] -= t.getResourceConsumption();
    }

    bool respectsResourceRestriction(const InstanceMatrix& instance) const {
        int vRes = instance.getVres();
        for (int v : resourcePerService_)
            if (v > vRes) return false;
        return true;
    }

    int getNumberOfEmployedServices() const { return numberOfEmployedServices_; }

    int getServiceForTask(int taskId) const {
        return allocation_[taskId];
    }

    // serviceId -> consumo total de recurso. Usado pela busca por oscilação
    // estratégica para medir sobrecarga de capacidade sem reconstruir.
    const vector<int>& getResourcePerService() const { return resourcePerService_; }

    // taskId -> serviceId; -1 quando não alocado.
    const vector<int>& getAllocation() const { return allocation_; }

    double getCurrentCost() const { return currentCost_; }
    void setCurrentCost(double cost) { currentCost_ = cost; }

    double getCurrentProb() const { return currentProb_; }
    void setCurrentProb(double prob) { currentProb_ = prob; }

    double getTimeToBest() const { return timeToBest_; }
    void setTimeToBest(double time) { timeToBest_ = time; }

private:
    void ensureSized(const InstanceMatrix& instance) {
        // As estruturas dependem do tamanho da instância e são alocadas apenas
        // quando a primeira tarefa é inserida.
        if (allocation_.empty()) {
            allocation_.assign(instance.getNumberOfTasks(), -1);
            resourcePerService_.assign(instance.getNumberOfServices(), 0);
            employedServices_.assign(instance.getNumberOfServices(), 0);
        }
    }

    vector<int> allocation_;          // taskId -> serviceId, -1 = não alocado
    vector<int> resourcePerService_;  // serviceId -> consumo total
    vector<int> employedServices_;    // serviceId -> contagem de tarefas

    double currentCost_;
    double currentProb_;
    double timeToBest_;

    int numberOfTasksAllocated_;
    int numberOfEmployedServices_;
};
