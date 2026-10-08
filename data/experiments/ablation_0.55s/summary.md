# Ablação das melhorias da GLS — 0,55 s fixos por execução

- Instâncias: 94; repetições: 3 (mesma solução inicial por instância, sementes distintas).
- Orçamento: 0,55 s fixos por execução; encerra antes se atinge o ótimo conhecido.
- Fatores (todos sobre GFLS): I = avaliação incremental, S = substituição composta de serviço, L = lambda local, R = utilidade por arrependimento. Baseline extra: GLS pura (sem GFLS).
- Tempo médio = tempo até parar (ótimo encontrado ou 0,55 s esgotados), média das 282 execuções.

## Construção acumulada até a configuração promovida

| Etapa | Configuração | Instâncias no ótimo | Execuções ótimas | GAP médio | Tempo médio |
|---|---|---:|---:|---:|---:|
| GLS pura | GLS pura (sem GFLS) | 14/94 | 41/282 | 7,63% | 0,508 s |
| + GFLS | I0 S0 L0 R0 | 15/94 | 44/282 | 7,52% | 0,506 s |
| + Avaliação incremental | I1 S0 L0 R0 | 66/94 | 194/282 | 2,10% | 0,311 s |
| + Substituição de serviço | I1 S1 L0 R0 | 36/94 | 108/282 | 2,19% | 0,386 s |
| + Arrependimento (promovida) | I1 S1 L0 R1 | 89/94 | 265/282 | 0,30% | 0,123 s |

## Remoção de cada melhoria a partir da promovida (I1 S1 L0 R1)

| Variação | Instâncias no ótimo | Δ vs promovida | GAP médio | Δ GAP (pp) | Tempo médio | Instâncias piores / melhores / iguais |
|---|---:|---:|---:|---:|---:|---|
| Sem avaliação incremental (I0 S1 L0 R1) | 82/94 | -7 | 1,00% | +0.71 | 0,325 s | 12 / 0 / 82 |
| Sem substituição de serviço (I1 S0 L0 R1) | 13/94 | -76 | 6,98% | +6.68 | 0,484 s | 81 / 0 / 13 |
| Sem utilidade por arrependimento (I1 S1 L0 R0) | 36/94 | -53 | 2,19% | +1.89 | 0,386 s | 58 / 0 / 36 |
| Com lambda local (rejeitado) (I1 S1 L1 R1) | 86/94 | -3 | 0,31% | +0.01 | 0,199 s | 6 / 1 / 87 |

## Interação substituição de serviço × arrependimento (I1, L0)

| | Sem arrependimento (R0) | Com arrependimento (R1) |
|---|---:|---:|
| Sem substituição (S0) | 66/94, GAP 2,10% | 13/94, GAP 6,98% |
| Com substituição (S1) | 36/94, GAP 2,19% | 89/94, GAP 0,30% |

## Ranking das 17 configurações

| # | Configuração | Instâncias no ótimo | Execuções ótimas | Custo médio | GAP médio | Tempo médio |
|---:|---|---:|---:|---:|---:|---:|
| 1 | I1 S1 L0 R1 ★ | 89/94 | 265/282 | 100,89 | 0,30% | 0,123 s |
| 2 | I1 S1 L1 R1 | 86/94 | 258/282 | 100,89 | 0,31% | 0,199 s |
| 3 | I1 S1 L1 R0 | 85/94 | 255/282 | 101,39 | 0,80% | 0,238 s |
| 4 | I0 S1 L0 R1 | 82/94 | 246/282 | 101,60 | 1,00% | 0,325 s |
| 5 | I0 S1 L1 R0 | 83/94 | 248/282 | 101,84 | 1,25% | 0,494 s |
| 6 | I1 S0 L1 R0 | 85/94 | 255/282 | 102,33 | 1,73% | 0,098 s |
| 7 | I0 S1 L1 R1 | 62/94 | 184/282 | 102,51 | 1,91% | 0,474 s |
| 8 | I1 S0 L0 R0 | 66/94 | 194/282 | 102,70 | 2,10% | 0,311 s |
| 9 | I1 S1 L0 R0 | 36/94 | 108/282 | 102,79 | 2,19% | 0,386 s |
| 10 | I0 S1 L0 R0 | 31/94 | 92/282 | 103,23 | 2,63% | 0,448 s |
| 11 | I0 S0 L1 R0 | 84/94 | 252/282 | 103,42 | 2,81% | 0,352 s |
| 12 | I1 S0 L1 R1 | 13/94 | 39/282 | 107,57 | 6,95% | 0,488 s |
| 13 | I1 S0 L0 R1 | 13/94 | 39/282 | 107,61 | 6,98% | 0,484 s |
| 14 | I0 S0 L0 R0 | 15/94 | 44/282 | 108,15 | 7,52% | 0,506 s |
| 15 | GLS pura (sem GFLS) | 14/94 | 41/282 | 108,27 | 7,63% | 0,508 s |
| 16 | I0 S0 L0 R1 | 11/94 | 33/282 | 109,07 | 8,43% | 0,506 s |
| 17 | I0 S0 L1 R1 | 10/94 | 30/282 | 109,93 | 9,29% | 0,518 s |

## Efeitos principais (fatorial 2⁴, média do GAP)

| Fator | GAP desligado | GAP ligado | Redução (pp) |
|---|---:|---:|---:|
| Avaliação incremental | 4,36% | 2,67% | +1.69 |
| Substituição de serviço | 5,73% | 1,30% | +4.43 |
| Lambda local | 3,89% | 3,13% | +0.76 |
| Utilidade por arrependimento | 2,63% | 4,40% | -1.77 |

## Interações pareadas (sinergia, pp de GAP)

Positivo = ligar os dois juntos reduz o GAP mais do que a soma dos efeitos isolados.

| Par | Sinergia |
|---|---:|
| Avaliação incremental × Substituição de serviço | -0.89 |
| Avaliação incremental × Lambda local | -0.32 |
| Avaliação incremental × Utilidade por arrependimento | -0.16 |
| Substituição de serviço × Lambda local | -0.30 |
| Substituição de serviço × Utilidade por arrependimento | +2.60 |
| Lambda local × Utilidade por arrependimento | -1.20 |
