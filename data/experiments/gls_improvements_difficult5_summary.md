# Ablação das melhorias GLS/GFLS — cinco instâncias difíceis

- Instâncias: 28, 100, 128, 129, 147
- Repetições: 3
- Orçamento por variante: 0.550 s
- SLA, capacidade e Smax permaneceram restrições duras.

| Etapa | Custo médio | GAP médio | Candidatos/s | Rodadas/s |
|---|---:|---:|---:|---:|
| GLS | 141.80 | 40.65% | 36177516 | 594.1 |
| +GFLS | 141.80 | 40.65% | 38118575 | 621.9 |
| +Avaliação incremental | 127.60 | 26.56% | 263485817 | 4145.3 |
| +Substituição de serviço | 114.00 | 13.08% | 95444218 | 865.6 |
| +Lambda local | 114.00 | 13.07% | 100465812 | 1353.6 |
| +Utilidade por arrependimento | 106.20 | 5.35% | 175236292 | 3851.4 |
| +Penalização top-1 | 107.07 | 6.20% | 215580632 | 5605.7 |
| +Oscilação condicional | 107.13 | 6.27% | 216708685 | 5635.5 |

## Contribuição marginal

- +GFLS: +0.00 unidade(s) de custo médio.
- +Avaliação incremental: +14.20 unidade(s) de custo médio.
- +Substituição de serviço: +13.60 unidade(s) de custo médio.
- +Lambda local: +0.00 unidade(s) de custo médio.
- +Utilidade por arrependimento: +7.80 unidade(s) de custo médio.
- +Penalização top-1: -0.87 unidade(s) de custo médio.
- +Oscilação condicional: -0.07 unidade(s) de custo médio.

## Oscilação condicional

- Chamadas: 0
- Chamadas que melhoraram a solução corrente: 0
- O gatilho não disparou porque capacidade não representou 60% dos bloqueios; Smax foi o bloqueador dominante.
- A SLA não bloqueou movimentos promissores nas cinco instâncias deste protocolo.

## Decisão para a configuração padrão

- Promovidos provisoriamente: GFLS, avaliação incremental, substituição composta de serviço, lambda local e utilidade por arrependimento.
- A utilidade por arrependimento só apresentou ganho forte depois da inclusão da vizinhança composta, indicando interação entre as melhorias.
- Mantidos somente como opções experimentais: penalização top-1 e oscilação condicional, pois não melhoraram o resultado agregado.
- A configuração promovida ainda precisa ser validada nas 94 instâncias.
- As repetições medem variação de tempo; a trajetória atual é majoritariamente determinística nestas cinco instâncias.
