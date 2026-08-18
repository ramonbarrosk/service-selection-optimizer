#pragma once

// Representa uma tarefa do problema e seu consumo fixo de recurso.
// A decisão de qual serviço a executará fica armazenada em Allocation.
class Task {
public:
    Task(int id, int resourceConsumption)
        : taskId_(id), resourceConsumption_(resourceConsumption) {}

    int getTaskId() const { return taskId_; }
    void setTaskId(int id) { taskId_ = id; }

    int getResourceConsumption() const { return resourceConsumption_; }
    void setResourceConsumption(int consumption) { resourceConsumption_ = consumption; }

    bool operator==(const Task& other) const { return taskId_ == other.taskId_; }

private:
    int taskId_;
    int resourceConsumption_;
};
