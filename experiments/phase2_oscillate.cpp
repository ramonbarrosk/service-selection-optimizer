// Fase 2: testa as propostas (1) construtivo best-fit cheapest-feasible e
// (2) busca local com OSCILACAO ESTRATEGICA (aceita violar capacidade sob
// penalidade adaptativa) para ver se quebram o teto de ~70% na instancia 100.
//
// Mede, por instancia:
//   (B)  best-fit guloso (1 construcao)            -> baseline ja conhecido
//   (D)  best-fit + VND atual (cost-only)          -> teto atual ~70%
//   (F)  best-fit + oscillating search             -> proposta (2)
//   (G)  multistart best-fit-RCL + oscillating x N -> proposta (1)+(2)

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <numeric>
#include <algorithm>
#include <limits>
#include <chrono>

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

// ---- construtivo best-fit decreasing + cheapest-feasible (alpha>0 => RCL) ----
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
        vector<std::pair<int,int>> svc; svc.reserve(S);
        for (int s=0;s<S;s++) svc.emplace_back(m.getTaskCost(taskId,s), s);
        std::sort(svc.begin(),svc.end());
        int minC=svc.front().first, maxC=svc.back().first;
        int thr = minC + (int)(alpha*(maxC-minC));
        vector<int> candIdx;
        for (size_t i=0;i<svc.size();++i) if (svc[i].first<=thr || alpha==0.0) candIdx.push_back((int)i);
        if (alpha>0.0) std::shuffle(candIdx.begin(),candIdx.end(),RandomUtil::engine());
        for (size_t i=0;i<svc.size();++i)
            if (std::find(candIdx.begin(),candIdx.end(),(int)i)==candIdx.end()) candIdx.push_back((int)i);
        bool placed=false;
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

// ---------------------- busca local por oscilacao estrategica ----------------------
// Objetivo penalizado: f = custo + lambda * sobrecarga_de_capacidade.
// Capacidade (Vres por servico) vira RESTRICAO SUAVE; Smax e SLA seguem DURAS.
// Permite atravessar a regiao inviavel-por-capacidade para reequilibrar e baratear.
struct Oscillator {
    const InstanceMatrix& m;
    SolutionValidator val;
    ProbabilityScenario sc = ProbabilityScenario::Ps;
    int Vres, Vmax, Smax; double Pmax;

    Oscillator(const InstanceMatrix& mm)
        : m(mm), Vres(mm.getVres()), Vmax(mm.getVmax()), Smax(mm.getSmax()), Pmax(mm.getPmax()) {}

    long overload(const Allocation& a) const {
        long ov = 0;
        for (int u : a.getResourcePerService()) if (u > Vres) ov += (u - Vres);
        return ov;
    }
    // SLA viavel? (so precisa checar quando a prob pode ter piorado)
    bool slaOk(Allocation& a, int oldServ, int newServ) {
        if (m.getServiceProb(newServ) <= m.getServiceProb(oldServ)) return true; // prob nao piora
        return val.computeViolationExcess(m, a, Vmax, Pmax, sc) <= 1e-12;
    }

    // Descida best-improvement no objetivo penalizado f. Retorna true se mexeu.
    bool penalizedDescent(Allocation& a, double lambda) {
        bool any=false, improved=true;
        int n=m.getNumberOfTasks(), S=m.getNumberOfServices();
        while (improved) {
            improved=false;
            int bestT=-1,bestS=-1; double bestDelta=-1e-9;
            const auto& alloc=a.getAllocation();
            for (int t=0;t<n;t++){
                int c=alloc[t]; if(c<0) continue;
                int cons=m.getTaskConsumption(t);
                const auto& load=a.getResourcePerService();
                long usedC=load[c];
                long ovC_old=std::max(0L,usedC-(long)Vres);
                long ovC_new=std::max(0L,usedC-cons-(long)Vres);
                for (int s=0;s<S;s++){
                    if(s==c) continue;
                    double dCost=m.getTaskCost(t,s)-m.getTaskCost(t,c);
                    long usedS=load[s];
                    long ovS_old=std::max(0L,usedS-(long)Vres);
                    long ovS_new=std::max(0L,usedS+cons-(long)Vres);
                    double dOver=(ovC_new-ovC_old)+(ovS_new-ovS_old);
                    double dF=dCost+lambda*dOver;
                    if(dF<bestDelta){
                        // valida Smax e SLA aplicando o move
                        Task task(t,cons); Service ns(s), cs(c);
                        a.replaceService(task,ns,m);
                        bool ok = a.getNumberOfEmployedServices()<=Smax && slaOk(a,c,s);
                        a.replaceService(task,cs,m);
                        if(ok){ bestDelta=dF; bestT=t; bestS=s; }
                    }
                }
            }
            if(bestT>=0){
                a.replaceService(Task(bestT,m.getTaskConsumption(bestT)),Service(bestS),m);
                improved=true; any=true;
            }
        }
        return any;
    }

    // Perturbacao: k movimentos aleatorios mantendo Smax (capacidade pode violar,
    // a oscilacao conserta). Diversifica o ponto de partida da proxima oscilacao.
    void perturb(Allocation& a, int k) {
        int n=m.getNumberOfTasks(), S=m.getNumberOfServices();
        for(int j=0;j<k;j++){
            int t=RandomUtil::getRandomInt(0,n-1);
            int s=RandomUtil::getRandomInt(0,S-1);
            Task task(t,m.getTaskConsumption(t)); Service ns(s), cs(a.getServiceForTask(t));
            a.replaceService(task,ns,m);
            if(a.getNumberOfEmployedServices()>Smax) a.replaceService(task,cs,m); // so Smax e duro aqui
        }
    }

    // ILS com oscilacao: perturba -> oscila -> guarda melhor viavel.
    Allocation ils(Allocation start, int iters, int rounds) {
        Allocation cur = run(start, rounds);
        Allocation best = cur; double bestCost = overload(cur)==0 ? cur.getCurrentCost() : std::numeric_limits<double>::max();
        for(int i=0;i<iters;i++){
            Allocation cand = cur;
            perturb(cand, 2 + RandomUtil::getRandomInt(0,3));
            cand = run(cand, rounds);
            if(overload(cand)==0 && cand.getCurrentCost()<bestCost){
                best=cand; bestCost=cand.getCurrentCost(); cur=cand;
            } else if(overload(cand)==0 && cand.getCurrentCost()<cur.getCurrentCost()+5){
                cur=cand; // aceita lateral leve para diversificar
            }
        }
        return best;
    }

    // Oscilacao: alterna lambda para visitar a fronteira de viabilidade.
    Allocation run(Allocation a, int rounds) {
        double meanCost=0; int n=m.getNumberOfTasks();
        for(int t=0;t<n;t++) meanCost+=m.getMinCostForTask(t);
        meanCost/=std::max(1,n);
        double lambda=std::max(1.0, meanCost*2.0);

        Allocation best=a; double bestCost = overload(a)==0 ? a.getCurrentCost() : std::numeric_limits<double>::max();
        for(int r=0;r<rounds;r++){
            penalizedDescent(a,lambda);
            long ov=overload(a);
            if(ov==0){
                if(a.getCurrentCost()<bestCost){ best=a; bestCost=a.getCurrentCost(); }
                lambda*=0.6;                 // relaxa: deixa cruzar para inviavel-barato
                if(lambda<0.25) lambda=0.25;
            } else {
                lambda*=2.0;                 // aperta: empurra de volta para viavel
            }
        }
        // garante terminar viavel: descida final com lambda alto
        penalizedDescent(a, lambda*8.0);
        if(overload(a)==0 && a.getCurrentCost()<bestCost){ best=a; bestCost=a.getCurrentCost(); }
        return best;
    }
};

static string gap(double c,int opt){ return opt>0?"  gap="+std::to_string((int)(100.0*(c-opt)/opt))+"%":""; }

int main(int argc, char** argv){
    int inst = argc>1?std::stoi(argv[1]):100;
    int N    = argc>2?std::stoi(argv[2]):50;
    int rounds=argc>3?std::stoi(argv[3]):30;
    InstanceMatrix m = readInstance(inst);
    int opt = loadOptimal(inst);
    cout << "Instance "<<inst<<"  optimal="<<opt<<"  (N="<<N<<", rounds="<<rounds<<")"<<endl;

    bool ok=false;
    Allocation bf = buildCheapestFeasibleAlloc(m,0.0,ok);
    if(!ok){ cout<<"best-fit FAILED"<<endl; return 1; }
    cout << "(B) best-fit guloso            : "<<(int)bf.getCurrentCost()<<gap(bf.getCurrentCost(),opt)<<endl;

    {
        Allocation r=GenericSearcher().VND(bf,m,m.getVmax(),m.getSmax(),m.getPmax(),
                                           ProbabilityScenario::Ps,ImprovementCondition::BEST_IMPROVEMENT);
        cout << "(D) best-fit + VND (cost-only) : "<<(int)r.getCurrentCost()<<gap(r.getCurrentCost(),opt)<<endl;
    }
    {
        auto t0=std::chrono::steady_clock::now();
        Allocation r=Oscillator(m).run(bf,rounds);
        auto dt=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();
        cout << "(F) best-fit + oscillation     : "<<(int)r.getCurrentCost()<<gap(r.getCurrentCost(),opt)
             << "  ("<<dt<<"s)"<<endl;
    }
    {
        // (G) ILS com oscilacao: N reinicios pareados por seed, cada um best-fit + ILS-osc.
        double best=std::numeric_limits<double>::max(), sum=0; int succ=0;
        auto t0=std::chrono::steady_clock::now();
        for(int r=0;r<N;r++){
            RandomUtil::setSeed(r); bool o;
            Allocation a=buildCheapestFeasibleAlloc(m,0.0,o);
            if(!o) continue;
            Oscillator osc(m);
            Allocation s=osc.ils(a, rounds, 30);  // 'rounds' aqui = iteracoes de ILS
            if(osc.overload(s)==0){ succ++; double c=s.getCurrentCost(); sum+=c; if(c<best)best=c; }
        }
        auto dt=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();
        double mean = succ? sum/succ : 0;
        cout << "(G) ILS-osc x"<<N<<" (iters="<<rounds<<")    : best="<<(succ?std::to_string((int)best):"FAILED")
             << (succ?gap(best,opt):"")<<"  (feasible "<<succ<<"/"<<N<<", "<<dt<<"s)"<<endl;
        // linha machine-readable p/ o visualize.py: nome opt opt_time mean best time
        if(succ)
            cout << "RESULT Instance_10_10_"<<inst<<" "<<opt<<" 0 "
                 << mean<<" "<<best<<" "<<dt<<endl;
    }
    return 0;
}
