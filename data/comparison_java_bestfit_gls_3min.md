# Comparação com aproximadamente três minutos

As variantes C++ foram executadas com `SSO_TIME_SCALE=0.80` e deadline interno.
O tempo algorítmico informado foi 2min59,6s em ambas; carregamento e finalização
elevaram o tempo externo para aproximadamente 3min07s. O Java original medido
levou 3min01,18s.

| Solução | Tempo interno | Tempo externo | Atingiu ótimo (≥1/3) | Ótimo nas 3 | Média dos melhores | GAP médio dos melhores |
|---|---:|---:|---:|---:|---:|---:|
| Java original | não informado | 3min01,18s | 0/94 | indisponível | 164,34 | 63,38% |
| Best-fit + FLS + oscilação | 2min59,64s | 3min07,34s | **60/94** | **46/94** | 102,33 | 1,73% |
| Best-fit + FLS + oscilação + GLS/GFLS | 2min59,66s | 3min07,42s | 59/94 | 44/94 | **102,11** | **1,51%** |

Com três minutos, o best-fit sem GLS encontrou um ótimo a mais. O GLS, porém,
obteve melhor qualidade agregada nas instâncias não resolvidas: reduziu o GAP
médio em 0,22 ponto percentual e o custo médio dos melhores em 0,22 unidade.

Comparando o melhor custo por instância, GLS venceu 15, empatou 67 e perdeu 12
para o best-fit. Comparando a média das três repetições, venceu 25, empatou 49 e
perdeu 20.

## Cinco instâncias difíceis

| Instância | Ótimo | Java melhor | Best-fit melhor | GLS melhor |
|---:|---:|---:|---:|---:|
| 100 | 101 | 210 | 139 | **125** |
| 129 | 100 | 191 | **107** | **107** |
| 147 | 100 | 204 | **106** | 107 |
| 28 | 102 | 193 | 121 | **119** |
| 128 | 101 | 202 | 116 | **113** |

Nas difíceis, o GLS venceu três, empatou uma e perdeu uma. Nenhuma encontrou o
ótimo nesse subconjunto.
