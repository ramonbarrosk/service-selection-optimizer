// Probe: compara a QUALIDADE da solução inicial viável de tres construtores,
// para checar se existe caminho ate perto do otimo numa instancia apertada.
//
//   (A) prob-based  : o fallback deterministico atual (ordena por prob de SLA)
//   (B) cheapest-feasible (best-fit decreasing por recurso): ordena tarefas por
//       consumo decrescente e atribui o serviço mais BARATO que mantem viavel.
//   (C) cheapest-feasible randomizado (RCL sobre custo) -> varios sorteios.

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <numeric>
#include <algorithm>
#include <limits>

#include "basic/Allocation.h"
#include "instance/InstanceMatrix.hpp"
#include "validator/SolutionValidator.hpp"
#include "search/GenericSearcher.h"
#include "enum/ProbabilityScenario.h"
#include "enum/ImprovementCondition.h"
#include "util/RandomUtil.hpp"

using std::string; using std::vector; using std::cout; using std::endl;

static InstanceMatrix readInstance(int n) {
    string path = "data/instances/Instance_10_10_" + std::to_string(n);
    std::ifstream file(path);
    bool initLine = true; InstanceMatrix instance; string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line); vector<string> t; string tok;
        while (iss >> tok) t.push_back(tok);
        if (t.empty()) continue;
        if (initLine) {
            instance = InstanceMatrix(std::stoi(t[2]), std::stoi(t[1]), std::stoi(t[3]),
                                      std::stoi(t[5]), std::stod(t[4]), std::stoi(t[6]));
            initLine = false;
        } else if (t[0]=="r") instance.setResourceConsumption(std::stoi(t[1])-1, std::stoi(t[2]));
        else if (t[0]=="p") instance.setSlaViolationProbability(std::stoi(t[1])-1, std::stod(t[2]));
        else if (t[0]=="c") instance.setTaskCost(std::stoi(t[2])-1, std::stoi(t[1])-1, std::stoi(t[3]));
    }
    for (int i=0;i<instance.getNumberOfServices();i++) instance.setServResourceCapacity(i, instance.getVres());
    return instance;
}
static int loadOptimal(int n){ std::ifstream f("data/Log/Instance_10_10_"+std::to_string(n)); string last,l;
    while(std::getline(f,l)) last=l; auto p=last.rfind("= "); return p!=string::npos?std::stoi(last.substr(p+2)):0; }

// cheapest-feasible best-fit decreasing. alpha>0 => RCL randomizada sobre custo.
static Allocation buildCheapestFeasibleAlloc(const InstanceMatrix& m, double alpha, bool& ok);
static double buildCheapestFeasible(const InstanceMatrix& m, double alpha, bool& ok) {
    Allocation a = buildCheapestFeasibleAlloc(m, alpha, ok);
    return ok ? a.getCurrentCost() : -1;
}
static Allocation buildCheapestFeasibleAlloc(const InstanceMatrix& m, double alpha, bool& ok) {
    SolutionValidator val; ProbabilityScenario sc = ProbabilityScenario::Ps;
    int n = m.getNumberOfTasks(), S = m.getNumberOfServices();
    int Vmax=m.getVmax(), Smax=m.getSmax(); double Pmax=m.getPmax();
    Allocation a;
    vector<int> order(n); std::iota(order.begin(),order.end(),0);
    std::sort(order.begin(),order.end(),[&](int x,int y){ return m.getTaskConsumption(x) > m.getTaskConsumption(y); });
    ok = true;
    for (int taskId : order) {
        Task task(taskId, m.getTaskConsumption(taskId));
        vector<std::pair<int,int>> svc; svc.reserve(S);       // (cost, servId)
        for (int s=0;s<S;s++) svc.emplace_back(m.getTaskCost(taskId,s), s);
        std::sort(svc.begin(),svc.end());
        bool placed=false;
        // RCL de custo: candidatos cujo custo <= min + alpha*(max-min)
        int minC=svc.front().first, maxC=svc.back().first;
        int thr = minC + (int)(alpha*(maxC-minC));
        // tenta candidatos em ordem de custo; com alpha>0 embaralha o topo da RCL
        vector<int> candIdx;
        for (size_t i=0;i<svc.size();++i) if (svc[i].first<=thr || alpha==0.0) candIdx.push_back((int)i);
        if (alpha>0.0) std::shuffle(candIdx.begin(),candIdx.end(),RandomUtil::engine());
        // garante varrer todos por custo se a RCL falhar
        for (size_t i=0;i<svc.size();++i)
            if (std::find(candIdx.begin(),candIdx.end(),(int)i)==candIdx.end()) candIdx.push_back((int)i);
        for (int ci : candIdx) {
            Service service(svc[ci].second, svc[ci].first);
            a.addTask(task, service, m);
            if (val.isFeasible(m,a,Vmax,Smax,Pmax,sc,true)) { placed=true; break; }
            a.removeTask(task, m);
        }
        if (!placed){ ok=false; return a; }
    }
    return a;
}

int main(int argc, char** argv){
    int inst = argc>1?std::stoi(argv[1]):100;
    InstanceMatrix m = readInstance(inst);
    int opt = loadOptimal(inst);
    cout << "Instance "<<inst<<"  optimal="<<opt<<endl;

    bool ok=false;
    double bf = buildCheapestFeasible(m, 0.0, ok);
    cout << "(B) cheapest-feasible best-fit  : " << (ok?std::to_string((int)bf):"FAILED")
         << (ok&&opt>0?"  gap="+std::to_string((int)(100.0*(bf-opt)/opt))+"%":"") << endl;

    // (D) best-fit + VND (MOVE+SWAP) — busca local atual sobre um bom ponto de partida
    {
        bool o; Allocation a = buildCheapestFeasibleAlloc(m, 0.0, o);
        if (o) {
            Allocation r = GenericSearcher().VND(a, m, m.getVmax(), m.getSmax(), m.getPmax(),
                                                 ProbabilityScenario::Ps, ImprovementCondition::BEST_IMPROVEMENT);
            cout << "(D) best-fit + VND              : " << (int)r.getCurrentCost()
                 << (opt>0?"  gap="+std::to_string((int)(100.0*(r.getCurrentCost()-opt)/opt))+"%":"") << endl;
        }
    }

    // (E) multistart: melhor de 200 (best-fit RCL + VND)
    double best=std::numeric_limits<double>::max(); int succ=0;
    for (int r=0;r<200;r++){ RandomUtil::setSeed(r); bool o; Allocation a=buildCheapestFeasibleAlloc(m,0.3,o);
        if(o){ Allocation s=GenericSearcher().VND(a,m,m.getVmax(),m.getSmax(),m.getPmax(),
                    ProbabilityScenario::Ps, ImprovementCondition::BEST_IMPROVEMENT);
               succ++; if(s.getCurrentCost()<best)best=s.getCurrentCost(); } }
    cout << "(E) best-fit RCL + VND  x200     : best="<<(succ?std::to_string((int)best):"FAILED")
         << (succ&&opt>0?"  gap="+std::to_string((int)(100.0*(best-opt)/opt))+"%":"")
         << "  (feasible "<<succ<<"/200)"<<endl;
    return 0;
}
