# ILS#3 com melhorias × GLS — conjunto hard, 10 s

Ótimos: instâncias com ótimo provado alcançado em ao menos uma execução. GAP: custo médio contra a referência (ótimo, LB alcançado ou melhor solução conhecida entre todos os métodos). Inviáveis: execuções cuja solução reportada viola alguma restrição. Vitórias: instâncias em que o método teve custo médio menor que a GLS. p: Wilcoxon pareado exato e bilateral sobre o custo médio por instância.

| Método | Ótimos | GAP médio | Tempo até a melhor (s) | Inviáveis | Melhor que a GLS | Empates | Pior que a GLS | p |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| GLS | 46/68 | 0,61% | 4,49 | 0/225 | – | – | – | – |
| ILS#3 original (fase anterior) | 4/68 | 26,42% | 10,03 | 0/225 | 0 | 0 | 75 | 5,3e-23 |
| ILS#3 + incremental + substituição | 9/68 | 10,43% | 9,76 | 0/225 | 0 | 1 | 74 | 1,1e-22 |
| ILS#3 + incremental + substituição + keepBest | 2/68 | 15,13% | 10,05 | 0/225 | 7 | 0 | 68 | 1,1e-15 |
