# Fase 12: ILS#3 com as melhorias da GLS

## Pergunta

A vantagem da GLS vem das melhorias de implementação (avaliação incremental do SLA e
vizinhança de substituição de serviço) ou do mecanismo da GLS em si (penalidades com custo
de arrependimento)? Para separar os dois efeitos, as duas melhorias foram portadas para o
ILS#3 Java do projeto de referência, que foi comparado de novo com a GLS.

## Implementação (projeto Java, branch `feature/ils-melhorias`)

- `search/ImprovedSearcher.java`: busca local de melhor melhoria na vizinhança MOVE, com as
  mesmas regras do `GenericSearcher` original (só movimentos que reduzem o custo, varredura
  a partir de um deslocamento aleatório, SLA verificado só quando o novo serviço tem
  probabilidade maior, maior ganho estrito), mais duas opções:
  - **incremental (I):** distribuição truncada do número de violações mantida pela busca;
    um MOVE é verificado em O(1) (capacidade, Smax e SLA), recálculo completo a cada 32
    movimentos e após cada substituição;
  - **substituição (S):** troca um serviço empregado por um vazio, com todas as suas
    tarefas, escolhida junto com o MOVE pela regra de maior ganho.
- `ILS.java` e `ArticleResult.java`: `-Dsso.incremental`, `-Dsso.replacement` e
  `-Dsso.keepbest`, todas desligadas por padrão (ILS#3 original). O CSV ganhou a coluna
  `feasible`, que confere a solução reportada com a DP completa.
- **keepBest (K)** corrige dois defeitos do ILS original:
  1. a perturbação altera o próprio objeto da melhor solução, então o ILS devolve o último
     ótimo local, e não o melhor encontrado;
  2. a perturbação MOVE sorteia um serviço para decidir se verifica o SLA e aplica outro,
     sorteado de novo, o que pode gerar soluções que violam o SLA.

Teste de correção: 216 descidas a partir de soluções perturbadas, em 6 instâncias e nas 3
combinações de I e S, terminaram todas em solução viável pela DP completa e em ótimo local
verdadeiro para MOVE e substituição.

## Execução

```bash
SET=ref  TIME_SECONDS=0.55 VARIANTS="000 001 100 010 110 111" bash experiments/12_ils_melhorado/scripts/run_ils_variants.sh
SET=ref  TIME_SECONDS=10   VARIANTS="000 110 111"             bash experiments/12_ils_melhorado/scripts/run_ils_variants.sh
SET=hard TIME_SECONDS=10   VARIANTS="110 111"                 bash experiments/12_ils_melhorado/scripts/run_ils_variants.sh
python3 experiments/12_ils_melhorado/scripts/analyze_ils_variants.py
python3 experiments/12_ils_melhorado/scripts/plot_ils_variants.py      # gráficos em charts/
```

Gráficos em `charts/`:

- `gap_por_variante.png`: GAP médio de cada método nos três cenários, com escala própria em
  cada painel;
- `gap_por_perfil.png`: GAP médio por perfil das instâncias geradas;
- `c50_diferenca_por_instancia.png`: custo médio das duas variantes melhoradas em relação à
  GLS em cada instância C50, com as instâncias sem ótimo provado marcadas.

Protocolo da fase 07: 3 repetições por instância, parada ao alcançar o ótimo conhecido.
A GLS é a das fases 08 (0,55 s), 07 (10 s, referência) e 10 (10 s, geradas); o ILS#3
original das geradas é o da fase 10. Os resumos completos estão em `results/summary_*.md`.

As primeiras execuções com keepBest corrigiam só o defeito 1 e reportaram 21 soluções
inviáveis nas instâncias C50. Elas estão em `results/descartados/` e não entram na análise.

## Resultados (11/11/2026)

94 instâncias de referência, 10 s:

| Método | Ótimos | GAP médio | Pior que a GLS / empates / melhor | p |
|---|---:|---:|---|---:|
| GLS | 93/94 | 0,04% | – | – |
| ILS#3 original | 86/94 | 3,40% | 19 / 75 / 0 | 3,8e-6 |
| ILS#3 + I + S | 86/94 | 1,31% | 9 / 85 / 0 | 0,004 |
| ILS#3 + I + S + K | 85/94 | 1,82% | 12 / 82 / 0 | 4,9e-4 |

94 instâncias de referência, 0,55 s:

| Método | Ótimos | GAP médio | Pior que a GLS / empates / melhor | p |
|---|---:|---:|---|---:|
| GLS | 89/94 | 0,30% | – | – |
| ILS#3 original | 3/94 | 21,59% | 93 / 1 / 0 | 2,0e-28 |
| ILS#3 + K | 6/94 | 20,35% | 94 / 0 / 0 | 1,0e-28 |
| ILS#3 + S | 61/94 | 4,04% | 76 / 18 / 0 | 2,6e-23 |
| ILS#3 + I | 65/94 | 8,52% | 74 / 20 / 0 | 1,1e-22 |
| ILS#3 + I + S | 65/94 | 4,06% | 76 / 18 / 0 | 2,6e-23 |
| ILS#3 + I + S + K | 80/94 | 2,97% | 26 / 68 / 0 | 3,0e-8 |

75 instâncias geradas, 10 s (GAP contra o ótimo provado ou a melhor solução conhecida entre
todos os métodos; as novas soluções do ILS + I + S + K no C50 elevam o GAP da GLS de 0,43%
para 0,61%):

| Método | Ótimos provados | GAP médio | Pior que a GLS / empates / melhor | p |
|---|---:|---:|---|---:|
| GLS | 46/68 | 0,61% | – | – |
| ILS#3 original | 4/68 | 26,42% | 75 / 0 / 0 | 5,3e-23 |
| ILS#3 + I + S | 9/68 | 10,43% | 74 / 1 / 0 | 1,1e-22 |
| ILS#3 + I + S + K | 2/68 | 15,13% | 68 / 0 / 7 | 1,1e-15 |

Por perfil, ILS#3 + I + S + K contra a GLS: perde em todas as 60 instâncias de T97 a T100
(p = 6,1e-5 em cada perfil), mas no C50 vence em 7 e perde em 8 (p = 0,67). As 7 vitórias
são as instâncias 1502, 1504, 1506, 1507, 1508, 1509 e 1513, todas com solução viável. Seis
delas não têm ótimo provado, e o ILS estabelece novas melhores soluções conhecidas.

## Conclusões

- As duas melhorias não são específicas da GLS: no ILS, elas reduzem o GAP de 3,40% para
  1,31% nas instâncias de referência e de 26,4% para 10,4% nas geradas.
- Ainda assim, o ILS melhorado não supera a GLS nas instâncias de referência nem nos
  perfis de capacidade apertada, em que o ótimo quase sempre é igual ao limite inferior
  trivial. Nesses casos, a diferença restante se deve ao mecanismo da GLS.
- No perfil C50, em que o ótimo é não trivial, o ILS corrigido e melhorado empata
  estatisticamente com a GLS e encontra soluções melhores em 7 das 15 instâncias. A
  vantagem da GLS sobre o ILS#3 original nesse perfil se deve às melhorias e aos defeitos do
  ILS original, e não ao mecanismo da GLS.
- keepBest só ajuda junto com as melhorias e com orçamento curto (65 para 80 ótimos com
  0,55 s). Com 10 s, ele piora a variante melhorada nos perfis T, provavelmente porque
  reiniciar sempre da melhor solução reduz a diversificação, que no ILS original vinha,
  sem intenção, de aceitar qualquer ótimo local.
