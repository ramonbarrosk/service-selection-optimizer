# Construção Gulosa Adaptativa Guiada por `sem_melhora`

## 1. Contexto: o algoritmo base (GRASP + ILS)

O algoritmo resolve um problema de seleção de serviços em nuvem com três restrições simultâneas: custo total, capacidade de recursos e probabilidade de violação de SLA. A estrutura geral é:

```
Solução inicial (construtivo guloso)
        ↓
  Busca local / VND
        ↓
  ┌─── Perturbação ←──────────────────┐
  │         ↓                         │
  │    Busca local / VND              │
  │         ↓                         │
  │  Melhorou? → salva melhor         │
  │  Não melhorou N vezes?            │
  │    → Mergulho inviável (IGrAl)    │
  │    → Falhou? Reinicia do greedy ──┘
  └──────────────────────────────────
```

O **construtivo guloso** é o ponto de partida de cada reinício. A qualidade e diversidade dessa construção influencia diretamente quais regiões do espaço de soluções serão exploradas.

---

## 2. O construtivo original: guloso por custo (GRASP)

O construtivo original usa a lógica de **Lista Restrita de Candidatos (RCL)** do GRASP:

1. Escolhe uma tarefa aleatória da lista de tarefas pendentes.
2. Calcula o custo mínimo e máximo entre todos os serviços disponíveis para essa tarefa.
3. Monta a RCL com os serviços cujo custo está dentro do limiar:

   ```
   threshold = minCusto + α × (maxCusto − minCusto)
   ```

4. Sorteia aleatoriamente um serviço da RCL.
5. Se a alocação violar alguma restrição, desfaz e tenta de novo.

O parâmetro `α` controla a **ganância vs. aleatoriedade**:
- `α = 0` → sempre o serviço mais barato (puramente guloso)
- `α = 1` → qualquer serviço (puramente aleatório)

**Problema:** todos os reinicios exploram regiões similares do espaço — aquelas com custo baixo. Quando o algoritmo fica preso num ótimo local por várias tentativas seguidas, reiniciar sempre pelo mesmo critério dificilmente leva a uma região nova.

---

## 3. A intuição da mudança

Considere que há duas dimensões naturais para selecionar um serviço:

| Critério | O que mede | Objetivo |
|---|---|---|
| **Custo** | quanto custa alocar a tarefa neste serviço | minimizar |
| **Probabilidade de violação de SLA** | risco de o serviço não cumprir o nível de serviço | minimizar |

O construtivo original só considera custo. Uma construção guiada por probabilidade gera soluções estruturalmente diferentes — que colocam as tarefas nos serviços mais confiáveis, independentemente do custo. A busca local subsequente pode encontrar, a partir desse novo ponto de partida, ótimos locais que estavam "escondidos" para o construtivo por custo.

A ideia central é: **usar o número de reinicios consecutivos sem melhora como sinal de estagnação** e, à medida que esse sinal cresce, deslocar progressivamente o critério de seleção do custo para a probabilidade.

---

## 4. O peso adaptativo `w`

Definimos um peso `w ∈ [0, 1]` que controla a mistura entre os dois critérios:

```
w = min(1.0,  restartStreak / 5.0)
```

onde `restartStreak` conta quantos reinicios do greedy consecutivos não resultaram em melhora da melhor solução global.

| `restartStreak` | `w` | Comportamento do construtivo |
|---|---|---|
| 0 | 0.0 | idêntico ao original — 100% guiado por custo |
| 1 | 0.2 | 80% custo, 20% probabilidade |
| 2 | 0.4 | 60% custo, 40% probabilidade |
| 3 | 0.6 | 40% custo, 60% probabilidade |
| 4 | 0.8 | 20% custo, 80% probabilidade |
| ≥ 5 | 1.0 | 100% guiado por probabilidade |

`restartStreak` é zerado sempre que o algoritmo encontra uma nova melhor solução global, fazendo o construtivo voltar a priorizar custo.

---

## 5. O score blended e a nova RCL

Para cada serviço `s` candidato à tarefa `i`, calcula-se um **score normalizado**:

```
custo_norm(i,s)  = ( custo(i,s)   − min_custo(i) )  /  ( max_custo(i)  − min_custo(i) )
prob_norm(s)     = ( prob(s)       − min_prob     )  /  ( max_prob      − min_prob     )

score(i,s) = (1 − w) × custo_norm(i,s)  +  w × prob_norm(s)
```

- Ambas as componentes ficam no intervalo `[0, 1]` após normalização.
- Valores menores de score são melhores (queremos custo baixo E probabilidade baixa).
- `custo_norm` é específico por tarefa (cada tarefa tem seu próprio intervalo de custos).
- `prob_norm` é global (o intervalo de probabilidade é o mesmo para todas as tarefas, pois a probabilidade pertence ao serviço).

A RCL é então montada com o mesmo mecanismo GRASP original, agora sobre o score blended:

```
threshold = min_score + α × (max_score − min_score)
RCL = { s : score(i,s) ≤ threshold }
```

Um serviço da RCL é sorteado aleatoriamente. O restante da lógica (verificação de viabilidade, retentativa) permanece idêntico ao construtivo original.

---

## 6. Integração com o ILSWithRestart

O fluxo modificado no ILS:

```
restartStreak = 0

loop ILS:
    perturbação → busca local
    
    melhorou melhor global?
        sim → restartStreak = 0   (volta ao construtivo por custo)
        não → contNotImproved++
    
    contNotImproved > IT_MAX/10?
        sim → contNotImproved = 0
              tenta mergulho inviável (IGrAl)
              
              w = min(1.0, restartStreak / 5.0)
              
              mergulho promissor E VND retornou à viabilidade?
                  melhorou melhor global? → restartStreak = 0
                  (não reinicia o greedy, continua da solução explorada)
              caso contrário:
                  currentAllocation = adaptiveGreedy(..., w)
                  restartStreak++
```

**Ponto importante:** `restartStreak` só incrementa quando há um reinício real do construtivo. Quando o mergulho inviável encontra uma solução melhor sem precisar reiniciar, o `restartStreak` não muda — o algoritmo está se saindo bem e não há motivo para alterar o critério de construção.

---

## 7. Comportamento esperado

**Instâncias fáceis / início da busca:**
O `restartStreak` permanece baixo (o algoritmo melhora com frequência), então `w ≈ 0` e o comportamento é praticamente idêntico ao original.

**Instâncias difíceis / algoritmo preso:**
Após vários reinicios frustrados, `w` cresce e o construtivo passa a priorizar serviços mais confiáveis (menor probabilidade de violação de SLA). Isso posiciona a solução inicial em regiões do espaço que o construtivo por custo não explora naturalmente, dando à busca local a chance de encontrar ótimos locais diferentes.

**Após encontrar melhora:**
`restartStreak = 0` → o construtivo volta imediatamente a priorizar custo. A memória de estagnação não persiste além da última melhora.

---

## 8. Propriedades da implementação

- **Retaguarda segura:** quando `w = 0`, `adaptiveGreedyInitialSolution` delega diretamente para `greedyInitialSolution` — zero overhead e comportamento idêntico ao original.
- **Fallback inalterado:** se o construtivo adaptativo travar (mais de `3 × nTarefas` tentativas), cai no `ProbabilityBasedInitialSolution` exatamente como o original.
- **Sem novos parâmetros expostos:** `w` é calculado internamente a partir de `restartStreak`; a interface pública do ILS não muda.
- **Normalização robusta:** se todos os serviços tiverem o mesmo custo ou a mesma probabilidade, o denominador é forçado a `1.0` para evitar divisão por zero.
