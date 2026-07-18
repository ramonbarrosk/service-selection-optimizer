# Avaliação: o GRASP adaptativo funcionou? E dá pra chegar perto do ótimo na instância 100?

> Análise empírica do commit `04f54fc feat grasp adaptive prob`, isolando o efeito do
> construtivo adaptativo das demais mudanças, com foco na instância `Instance_10_10_100`.

---

## Veredito curto

1. **O GRASP adaptativo (`grasp-adaptive-prob`) NÃO teve efeito nenhum.** Em 30 execuções
   pareadas por seed na instância 100, a versão com adaptativo e a versão sem deram resultado
   **byte-a-byte idêntico** (min=279, média=394,33, sd=60,91 nas duas). Não é coincidência nem
   "ruído" — é zero efeito mensurável.
2. **A melhora observada veio das mudanças no `main.cpp` (mais iterações / multistart por
   tempo), não do adaptativo.** Mais iterações ajudam de verdade; o adaptativo não.
3. **A instância 100 é a mais difícil porque está 98,2% cheia de recurso** (consumo 536 de
   capacidade 546). É quase um *bin-packing* no limite da viabilidade.
4. **Chegar perto do ótimo (101) com a arquitetura atual: não.** Há um teto estrutural na
   busca local. É possível melhorar bastante, mas para *near-optimal garantido* nessa instância
   o caminho confiável é um solver exato/ILP.

---

## Como avaliei (isolando a variável)

O problema central da dúvida é que o commit `04f54fc` mexeu **em duas coisas ao mesmo tempo**:
a lógica adaptativa no `ILS.hpp` *e* 74 linhas no `main.cpp` (iterações, instâncias-alvo,
orçamento de tempo). Com isso é impossível saber quem causou o quê.

Como `adaptiveGreedyInitialSolution` vira o greedy original quando `w=0`, montei um **A/B
controlado** que troca só `w`:

- `ADAPTIVE` = código atual (`w = min(0.5, restartStreak/5)`)
- `BASELINE` = `-DDISABLE_ADAPTIVE` força `w=0`

Mesmo seed por run (adicionei `RandomUtil::setSeed`), mesmo nº fixo de iterações, sem orçamento
de tempo. Assim a **única** diferença entre as duas é o construtivo adaptativo.

---

## Resultado 1 — o adaptativo é um *no-op*

| Run | adaptive | baseline | | Run | adaptive | baseline |
|---|---|---|---|---|---|---|
| 0 | 296 | 296 | | … | … | … |
| 1 | 419 | 419 | | 29 | 279 | 279 |

**0 de 30 runs diferem.** A instrumentação explica por quê: na instância 100 o construtivo
adaptativo foi chamado **162 vezes com w>0** e nas **162** ele estourou o limite de tentativas
(`cont > 3·n`) e **caiu no mesmo `ProbabilityBasedInitialSolution`** — que é determinístico.
Ou seja, toda construção adaptativa falha em montar solução viável e volta exatamente para o
mesmo ponto do baseline.

```
restartBranchHits=210  divePromising=0  diveReturnedFeasible=0
adaptiveCalls_w>0=162   fallbackToProbBased: adaptive(w>0)=162   ← 100% fallback
```

Observações adicionais:

- `divePromising=0` em **todas** as instâncias: o "mergulho inviável" (IGrAl) nunca é
  considerado promissor — o `lambda = bestCost/Pmax` deixa a penalização sempre maior que
  `bestCost`. Esse mecanismo também está, na prática, morto.
- O doc (`docs/adaptive_greedy_construction.md`) diz `w = min(1.0, …)` mas o código usa
  `min(0.5, …)`. Divergência real — mas irrelevante enquanto a construção nem completa.

---

## Resultado 2 — foi as iterações, não o adaptativo

Escalando iterações na instância 100 (o adaptativo segue morto o tempo todo):

| Iterações | min | média | tempo |
|---|---|---|---|
| 2.000 | 278 | 474 | 0,6s |
| 10.000 | 296 | 404 | 2,0s |
| 40.000 | 236 | 303 | 6,3s |
| 100.000 | **183** | 246 | 13,7s |

Mais iterações ajudam de fato — mas a melhora vem do "moedor" perturbação+busca local,
**não** dos restarts (que são código morto). E mesmo com 100k iterações o gap mínimo ainda é
**81%** (183 vs 101).

---

## Por que a instância 100 é a mais difícil

| Instância | Capacidade (Smax·Vres) | Consumo total | Folga | Ótimo |
|---|---|---|---|---|
| 100 | 39·14 = 546 | 536 | **10 (1,8%)** | 101 |
| 11 | 43·15 = 645 | 605 | 40 (6,2%) | 100 |

A 100 está **98,2% cheia**. Empacotar 100 tarefas em pouquíssima folga, respeitando ainda o
SLA (Pmax≈0,34), é quase um bin-packing no limite. O ótimo de 101 (≈1 por tarefa) usa os
serviços baratos — mas o construtivo guloso por custo não consegue encaixá-los todos sem violar
capacidade, estoura as tentativas e recai na solução determinística cara baseada em
probabilidade. Daí o gap gigante.

---

## Dá pra chegar perto do ótimo? Onde está o teto

Prototipei um construtor melhor (best-fit decrescente por recurso + serviço mais barato viável)
e medi a qualidade:

| Abordagem | Inst. 100 (gap) | Inst. 28 | Inst. 11 | Inst. 128 | Inst. 129 |
|---|---|---|---|---|---|
| ILS atual, 100k iter | 183 (81%) | — | — | — | — |
| **(B)** best-fit guloso (1 construção) | 182 (80%) | 170 (66%) | 144 (44%) | 147 (45%) | 159 (59%) |
| **(D)** best-fit + VND atual | **172 (70%)** | 155 (51%) | 144 (44%) | 142 (40%) | 148 (48%) |
| (E) best-fit-RCL + VND ×200 | 223 | 178 | 153 | 170 | 158 |

Dois fatos decisivos:

- **Uma única** construção best-fit (182) já empata com **todo** o ILS atual a 100k iterações
  (183). O construtivo atual é o gargalo nº 1.
- Mas best-fit + a busca local atual satura em **172** — longe de 101. **O teto real é a busca
  local**: `costImprovement` só aceita movimentos que reduzem custo *estritamente* e mantêm
  viabilidade. Ela nunca faz o movimento lateral/de piora temporária que liberaria capacidade
  para depois consolidar — exatamente o que uma instância apertada exige.

**Conclusão honesta:** com os operadores atuais, **não** dá para chegar perto do ótimo na 100
— há um teto estrutural em ~70%. Dá para melhorar muito (trocar o construtivo já equivale a
todo o ILS de hoje), mas *near-optimal garantido* nessa instância tão apertada é incerto para
uma metaheurística de descida pura.

---

## Propostas, em ordem de impacto

1. **Trocar o construtivo** por best-fit-decrescente + cheapest-feasible (constraint-aware).
   Evidência: 1 construção ≈ todo o ILS atual. Impacto alto, esforço baixo.
2. **Fortalecer a busca local (o gargalo real):** aceitar movimentos que pioram custo mas
   liberam capacidade (oscilação estratégica), ou aceitação tipo *simulated annealing*/tabu
   para escapar do mínimo de custo. Sem isso, qualquer construtivo bom satura em ~70%.
3. **Consertar a diversificação:** hoje todo restart cai na mesma solução determinística →
   restarts são inúteis. Ou faça o adaptativo completar viável (RCL ciente de viabilidade, ou
   reparo em vez de reconstrução), ou troque por *path-relinking*. Aproveite e alinhe o `w`
   (0.5 vs 1.0 do doc).
4. **Reativar (ou remover) o mergulho IGrAl:** `divePromising=0` sempre → re-tunar `lambda` ou
   aceitar inviabilidade com penalidade adaptativa, senão é código morto.
5. **Para as instâncias mais apertadas (caso da 100): um ILP/matheurística.** O ótimo de
   referência (101) veio de solver exato em 216s. Para 100×100 com ótimo conhecido, esse é o
   caminho confiável para near-optimal — e dá *ground truth* para calibrar a metaheurística.

Recomendação: começar por **(1) + (2)** — maior retorno e atacam diretamente os dois gargalos
medidos.

---

## Fase 2 — aplicando as propostas (1) e (2): funcionou

Implementei as duas propostas de maior impacto e medi antes de integrar:

- **(1) Construtivo best-fit-decrescente + cheapest-feasible** (`buildCheapestFeasibleAlloc`).
- **(2) Busca local por oscilação estratégica** (`Oscillator`): a capacidade `Vres` vira
  restrição **suave** (objetivo penalizado `f = custo + λ·sobrecarga`, λ adaptativo);
  `Smax` e SLA continuam **duras**. Isso permite atravessar a região inviável-por-capacidade
  para reequilibrar — exatamente o movimento que a busca cost-only não fazia. Embrulhada num
  laço ILS (perturba → oscila → guarda melhor viável).

Resultado nas 5 instâncias (ótimo conhecido):

| Instância | Ótimo | Teto antigo (best-fit+VND) | best-fit+oscilação | **ILS-oscilação** | Gap final |
|---|---|---|---|---|---|
| 100 | 101 | 172 (70%) — ILS antigo a 100k: 183 (81%) | 161 (59%) | **143–147** | **41–45%** |
| 128 | 101 | 142 (40%) | 130 (28%) | **128** | **26%** |
| 129 | 100 | 148 (48%) | 145 (45%) | **133** | **33%** |
| 11  | 100 | 144 (44%) | 144 (44%) | **141** | **41%** |
| 28  | 102 | 155 (51%) | 155 (51%) | **154** | **50%** |

Conclusões:

- **A oscilação estratégica quebra o teto estrutural** identificado na Fase 1. Na instância 100
  o gap mínimo caiu de **81% → 41%**; nas tightest (100/128/129) o ganho é grande, nas mais
  folgadas (11/28) é modesto — coerente: a oscilação só rende onde a capacidade é o gargalo.
- O construtivo best-fit (proposta 1) sozinho já valia todo o ILS antigo; com a busca local
  nova (proposta 2) o conjunto **domina o ILS original em todas as instâncias**.
- **Mas ainda não é near-optimal** (101 → ~143 na 100, gap 41%). Confirma a Fase 1: descida
  pura plateia. Para *near-optimal garantido* nas mais apertadas, o caminho continua sendo
  ILP/matheurística (proposta 5).

Reprodução:

```bash
g++ -std=c++17 -O3 -DNDEBUG -Isrc -Isrc/basic -Isrc/enum -Isrc/instance -Isrc/util \
    -Isrc/validator -Isrc/search -Isrc/metaheuristic -o build/phase2 experiments/phase2_oscillate.cpp
./build/phase2 100 10 50      # <instância> <nº reinicios ILS> <iterações ILS-osc>
```

---

## Fase 3 — integração na `main` (híbrido, atrás de flag)

As propostas (1) e (2) foram integradas ao código de produção, **gated** pela flag de
compilação `ENABLE_OSCILLATION`. Depois do A/B justo (Fase 4) o híbrido virou o **padrão**
(`make` = híbrido; `make OSC=0` volta ao baseline original):

- `ILS::bestFitInitialSolution` — construtivo best-fit (proposta 1), usado na construção
  inicial e nos restarts (RCL α=0.3 para diversificar). Cai no guloso se não fechar viável.
- `GenericSearcher::oscillationImprovement` — busca por oscilação estratégica (proposta 2),
  chamada no `neighborhoodSearch` logo após a descida cost-only.

**Descoberta importante da integração:** a oscilação **sozinha não rende** — depende do
construtivo best-fit. Na `main`, na instância 100 o guloso cai no fallback prob-based
(estrutura ruim) e a oscilação a partir daí não melhora nada (resultado byte-idêntico ao
baseline). Só com best-fit + oscilação juntos o ganho aparece. As duas propostas são
**sinérgicas**.

Comparação `main`/ILS real (mesma `ILSWithRestart`, 5 runs, IT_MAX=2000):

| Instância | Ótimo | baseline (min) | **híbrido (min)** |
|---|---|---|---|
| 11  | 100 | 118 | **109** |
| 28  | 102 | 255 | **145** |
| 100 | 101 | 468 | **152** |
| 128 | 101 | 144 | **125** |
| 129 | 100 | 129 | **113** |

E nas instâncias **fáceis** o híbrido **mantém o ótimo** (testado em 31/51/57/75/82/84/123/1/
15/23 → 10/10 chegam a 100, igual ao baseline). Best-of-both: não regride o que já era bom e
conserta as apertadas.

Como compilar/rodar a `main`:

```bash
make                 # híbrido (padrão) — ENABLE_OSCILLATION ligado
make OSC=0           # baseline original, para comparação
./build/service-selection-optimizer
```

---

## Fase 4 — A/B justo (mesmo budget, só a flag muda)

As comparações anteriores misturavam configurações (a Fase 2 usou o protótipo a budget baixo;
a Fase 1 comparava "melhor de N"). A Fase 4 fecha isso com um A/B **honesto**: o *mesmo*
`main.cpp`, o *mesmo* orçamento (20 repetições, `optimalExecTime/10` por repetição, multistart
por tempo), variando **apenas** a flag `OSC`. É o número que vale para o paper.

| Instância | Ótimo | baseline (melhor) | gap | **híbrido (melhor)** | gap | Δ |
|---|---|---|---|---|---|---|
| 11  | 100 | 102 | 2,0%   | 104 | 4,0%  | +2 |
| 28  | 102 | 133 | 30,4%  | **125** | 22,5% | −8 |
| 100 | 101 | 227 | 124,8% | **143** | 41,6% | **−84** |
| 128 | 101 | 120 | 18,8%  | **115** | 13,9% | −5 |
| 129 | 100 | 107 | 7,0%   | **104** | 4,0%  | −3 |
| **Gap médio** | | | **36,6%** | | **17,2%** | **−19,4 pp** |

- **Gap médio caiu pela metade (36,6% → 17,2%)**, com ganho enorme na #100 (227 → 143).
- A única "piora" (#11: 102 → 104 no *melhor*) é ruído de outlier: no **custo médio das 20
  execuções** o híbrido vence em **5 de 5** (#11: 105,10 → 104,25). Ele é mais estável, não pior.
- **Custo de tempo real: 1,07×–1,65×** (não os 5–8× por iteração). Como a parada é por *tempo*,
  a oscilação não estica o relógio — apenas troca "muitos restarts rasos" por "menos restarts
  profundos" no mesmo budget. Ligar o híbrido praticamente não muda o wall-clock.

Reprodução (as duas saídas, mesma config):

```bash
make          && ./build/service-selection-optimizer | tee data/fair_hybrid.txt
make OSC=0    && ./build/service-selection-optimizer | tee data/fair_baseline.txt
PYTHONPATH=scripts python3 scripts/table_phase2.py --cpp-results data/fair_hybrid.txt   --output-dir data/charts_fair_hybrid
PYTHONPATH=scripts python3 scripts/table_phase2.py --cpp-results data/fair_baseline.txt --output-dir data/charts_fair_baseline
```

---

## Fundamentação para o paper — por que as duas peças funcionam

O algoritmo tem duas fases: **de onde se parte** (construtivo) e **como se melhora** (busca
local). Cada proposta conserta um gargalo distinto; juntas, se multiplicam.

### Peça 1 — construtivo best-fit-decrescente + cheapest-feasible

- **"decreasing":** ordena tarefas por consumo de recurso **decrescente** e aloca as maiores
  primeiro (sabedoria de *First-Fit-Decreasing* em bin-packing: encaixe as pedras grandes antes
  da areia, senão elas não cabem no fim).
- **"cheapest-feasible":** para cada tarefa, escolhe o serviço **mais barato que ainda respeita
  as 3 restrições** — olha viabilidade **antes** de alocar (*constraint-aware*), em vez de
  alocar cego e reparar depois.
- **Evidência-chave:** uma **única** construção best-fit (182 na #100) empata com **todo** o ILS
  antigo a 100k iterações (183). O construtivo probabilístico antigo partia de um ponto tão ruim
  que a busca gastava todo o esforço só consertando o início. Impacto alto, esforço baixo.

### Peça 2 — busca local por oscilação estratégica (o gargalo real)

- **O trava:** a busca cost-only só aceita movimento **viável E que reduza custo**. Isso a
  prende num mínimo local. Nas instâncias apertadas o gargalo não é custo, é **capacidade**.
- **O movimento que ela não faz:** mover uma tarefa para um serviço mais caro *agora* (piora o
  custo no passo) para **liberar capacidade** num serviço lotado, permitindo consolidar e
  **eliminar um serviço depois** (custo despenca). A cost-only nunca dá o primeiro passo, então
  nunca colhe o ganho — por isso **qualquer construtivo bom satura em ~70%**.
- **A solução:** a capacidade `Vres` vira restrição **suave** no objetivo penalizado
  `f = custo + λ·sobrecarga`, com λ **adaptativo** (sobe quando inviável, desce quando viável).
  A busca "respira" entrando e saindo da inviabilidade-de-capacidade e encontra rearranjos que a
  viável-estrita não alcança. `Smax` e SLA continuam **duras**.
- **Por que oscilação e não SA/tabu:** *simulated annealing* e tabu escapam de mínimos locais de
  forma **genérica**; a oscilação é **cirúrgica** — relaxa exatamente a restrição que é o
  gargalo medido (capacidade). Atacar o gargalo direto rende mais que perturbação genérica.

### A sinergia (não é soma, é produto)

A oscilação **sozinha é no-op** (resultado byte-idêntico ao baseline): ela precisa da estrutura
"empacotada" que só o best-fit produz. Bom mecânico sem bom carro não faz nada. As duas peças só
rendem **juntas** — foi a descoberta central da integração (Fase 3).

### A ressalva honesta

O híbrido é o **teto do que uma busca por vizinhança alcança**, mas **não chega ao ótimo** nas
mais apertadas (143 vs 101 na #100). A descida por vizinhança, por melhor que seja, **estaciona
em ótimo local** — não há mecanismo de *limite* que prove quão longe do ótimo se está. Para
fechar esse gap com garantia, só um método exato (ver abaixo).

---

## Trabalhos futuros — ILP / matheurística (proposta 5, detalhada)

A diferença conceitual é entre **descer** e **provar**.

- **Metaheurística** caminha pelo espaço de soluções e para quando todos os vizinhos são piores.
  Não sabe se achou o ótimo ou está a 40% dele — não há garantia.
- **Solver ILP** não caminha, **delimita**: mantém um *limite inferior* (relaxação linear — "o
  ótimo não pode ser melhor que isto") e um *limite superior* (melhor solução inteira achada).
  Via **branch-and-bound**, descarta regiões inteiras sem visitá-las ("nesta sub-árvore o melhor
  possível é 150, mas já tenho 143 → nem olho"). Quando os limites se encontram, **prova o
  ótimo global**.

Modelo do problema (variáveis binárias):

```
x[i][j] = 1 se a tarefa i usa o serviço j ;  y[j] = 1 se o serviço j é empregado

min  Σ custo[i][j]·x[i][j]
s.a. Σ_j x[i][j] = 1            ∀i          (cada tarefa em 1 serviço)
     Σ_i r[i]·x[i][j] ≤ Vres·y[j]  ∀j       (capacidade de recurso)
     Σ_j y[j] ≤ Smax                        (nº máximo de serviços)
     x, y ∈ {0,1}
```

Os ótimos de referência que já usamos (101 na #100, em 216s) vieram exatamente deste tipo de
solver — o *ground truth* já é fruto de ILP.

**O gargalo do modelo — a restrição de SLA:** custo, capacidade e Smax são lineares e diretos,
mas a de SLA (`P(violação) ≤ Pmax`, via DP Poisson-binomial) é **probabilística e não-linear**.
Um ILP puro não a engole; é preciso **linearizar** (aproximar por segmentos), tratar como
**chance-constraint** (programação estocástica), ou resolver só com as restrições lineares e
**filtrar/reparar** as soluções que violam SLA. É por isso que a proposta 5 é "esforço alto":
o trabalho não é chamar o solver, é modelar essa restrição.

**Matheurística (meio-termo prático):** usar o ILP em **subproblemas** dentro do loop
heurístico — *fix-and-optimize*: congela 90 tarefas na posição atual e deixa o solver rearranjar
**otimamente** as 10 mais conflitantes (subproblema minúsculo, resolvido em ms), iterando os
subconjuntos. Ganha-se **qualidade exata localmente** com **velocidade heurística globalmente**,
furando exatamente os mínimos locais onde o híbrido estaciona (o solver enxerga rearranjos de
várias tarefas *de uma vez*, algo que o MOVE/SWAP um-a-um nunca vê).

| Abordagem | Garante ótimo? | Velocidade | Esforço |
|---|---|---|---|
| Híbrido (1+2) | não (para em local) | rápido | ✅ feito |
| Matheurística (fix-and-optimize) | ótimo local forte | médio | 🔶 médio |
| ILP puro | **prova o ótimo global** | pode ser lento | 🔴 alto (modelar SLA) |

---

## Apêndice — reprodutibilidade

Scaffolding de experimento em `experiments/` (binários independentes da `main`, cada um com seu
próprio `main()` — não são compilados nem chamados pelo build de produção):

- `experiments/experiment.cpp` — A/B pareado por seed (ADAPTIVE vs BASELINE) e escala de iterações.
- `experiments/phase2_oscillate.cpp` — protótipo da Fase 2 (best-fit + oscilação + ILS, isolados).
- `experiments/probe_construct.cpp` — mede qualidade dos construtores (best-fit, best-fit+VND, etc.).
- `RandomUtil::setSeed` (aditivo, inócuo) e blocos `#ifdef INSTRUMENT` / `#ifdef DISABLE_ADAPTIVE`
  em `ILS.hpp` (guardados por flag — não afetam o build padrão).

> Nota: como os arquivos saíram de `src/`, o compile precisa de `-Isrc` (além dos `-Isrc/<sub>`)
> para resolver os includes no estilo `"basic/Allocation.h"`.

Compilação e execução:

```bash
FLAGS="-std=c++17 -O3 -DNDEBUG -Isrc -Isrc/basic -Isrc/enum -Isrc/instance -Isrc/util -Isrc/validator -Isrc/search -Isrc/metaheuristic"

# A/B pareado
g++ $FLAGS                    -o build/exp_adaptive experiments/experiment.cpp
g++ $FLAGS -DDISABLE_ADAPTIVE -o build/exp_baseline experiments/experiment.cpp
./build/exp_adaptive 100 30 10000     # <instância> <nº runs> <IT_MAX>
./build/exp_baseline 100 30 10000

# Instrumentação (contadores de fallback, restart, dive, etc.)
g++ $FLAGS -DINSTRUMENT -o build/exp_instr experiments/experiment.cpp
./build/exp_instr 100 30 10000

# Qualidade dos construtores
g++ $FLAGS -o build/probe experiments/probe_construct.cpp
./build/probe 100
```
