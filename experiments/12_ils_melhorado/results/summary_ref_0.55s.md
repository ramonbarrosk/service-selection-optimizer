# ILS#3 com melhorias × GLS — conjunto ref, 0.55 s

Ótimos: instâncias com ótimo provado alcançado em ao menos uma execução. GAP: custo médio contra a referência (ótimo, LB alcançado ou melhor solução conhecida entre todos os métodos). Inviáveis: execuções cuja solução reportada viola alguma restrição. Vitórias: instâncias em que o método teve custo médio menor que a GLS. p: Wilcoxon pareado exato e bilateral sobre o custo médio por instância.

| Método | Ótimos | GAP médio | Tempo até a melhor (s) | Inviáveis | Melhor que a GLS | Empates | Pior que a GLS | p |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| GLS | 89/94 | 0,30% | 0,12 | 0/282 | – | – | – | – |
| ILS#3 original | 3/94 | 21,59% | 0,97 | 0/282 | 0 | 1 | 93 | 2,0e-28 |
| ILS#3 + keepBest | 6/94 | 20,35% | 0,89 | 0/282 | 0 | 0 | 94 | 1,0e-28 |
| ILS#3 + substituição | 61/94 | 4,04% | 0,36 | 0/282 | 0 | 18 | 76 | 2,6e-23 |
| ILS#3 + incremental | 65/94 | 8,52% | 0,34 | 0/282 | 0 | 20 | 74 | 1,1e-22 |
| ILS#3 + incremental + substituição | 65/94 | 4,06% | 0,36 | 0/282 | 0 | 18 | 76 | 2,6e-23 |
| ILS#3 + incremental + substituição + keepBest | 80/94 | 2,97% | 0,18 | 0/282 | 0 | 68 | 26 | 3,0e-8 |
