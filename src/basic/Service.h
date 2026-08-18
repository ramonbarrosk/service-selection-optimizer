#pragma once

#include <string>

using std::string;
using std::to_string;

// Identifica um serviço candidato. A capacidade efetivamente usada pela
// otimização pertence à instância e à alocação; este objeto é uma referência leve.
class Service {
public:
    Service(int id, int capacity)
        : servId_(id), resourceCapacity_(capacity) {}

    explicit Service(int id)
        : servId_(id), resourceCapacity_(0) {}

    int getServId() const { return servId_; }
    void setServId(int id) { servId_ = id; }

    int getResourceCapacity() const { return resourceCapacity_; }
    void setResourceCapacity(int capacity) { resourceCapacity_ = capacity; }

    string toString() const { return to_string(servId_); }

    bool operator==(const Service& other) const { return servId_ == other.servId_; }

private:
    int servId_;
    int resourceCapacity_;
};
