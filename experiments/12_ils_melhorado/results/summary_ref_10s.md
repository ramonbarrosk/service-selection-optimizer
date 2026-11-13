# ILS#3 com melhorias × GLS — conjunto ref, 10 s

Ótimos: instâncias com ótimo provado alcançado em ao menos uma execução. GAP: custo médio contra a referência (ótimo, LB alcançado ou melhor solução conhecida entre todos os métodos). Inviáveis: execuções cuja solução reportada viola alguma restrição. Vitórias: instâncias em que o método teve custo médio menor que a GLS. p: Wilcoxon pareado exato e bilateral sobre o custo médio por instância.

| Método | Ótimos | GAP médio | Tempo até a melhor (s) | Inviáveis | Melhor que a GLS | Empates | Pior que a GLS | p |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| GLS | 93/94 | 0,04% | 0,16 | 0/282 | – | – | – | – |
| ILS#3 original | 86/94 | 3,40% | 2,36 | 0/282 | 0 | 75 | 19 | 3,8e-6 |
| ILS#3 + incremental + substituição | 86/94 | 1,31% | 1,04 | 0/282 | 0 | 85 | 9 | 0,004 |
| ILS#3 + incremental + substituição + keepBest | 85/94 | 1,82% | 1,54 | 0/282 | 0 | 82 | 12 | 4,9e-4 |
