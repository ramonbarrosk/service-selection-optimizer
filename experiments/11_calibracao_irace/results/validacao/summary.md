# Validação da GLS calibrada pelo irace

Configuração calibrada: alpha = 0.9971, rounds = 79, gfls = 1, replacement = 1, regret = 1, calibrate = 0, maxpen = 3.
Configuração atual: alpha = 0.3, rounds = 30, gfls = 1, replacement = 1, regret = 1, calibrate = 0, maxpen = 0.

Protocolo da fase 07: 3 repetições de 10 s, parando ao alcançar o ótimo conhecido. Wilcoxon pareado, exato e bilateral, sobre o custo médio por instância.

| Conjunto | Config. | Ótimo alcançado | GAP médio | Tempo até a melhor | Melhor que a outra | p-valor |
|---|---|---:|---:|---:|---:|---:|
| 94 instâncias do artigo | atual | 93/94 | 0,04% | 0,30 s | 0 | 1,000 |
| 94 instâncias do artigo | calibrada | 93/94 | 0,01% | 0,24 s | 1 |  |
| 75 instâncias difíceis (fase 10) | atual | 46/75 | 6,64% | 4,56 s | 12 | 0,160 |
| 75 instâncias difíceis (fase 10) | calibrada | 51/75 | 6,44% | 3,81 s | 19 |  |
