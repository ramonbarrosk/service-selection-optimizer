# Experimento fatorial 2^4 — melhorias GLS/GFLS

- Instâncias: 28, 100, 128, 129 e 147
- Repetições por combinação: 3
- Orçamento por execução: 0.55s
- Fatores: avaliação incremental (I), substituição composta (S), lambda local (L) e utilidade por arrependimento (R).

## Ranking das combinações

| Posição | Configuração | Custo médio | GAP médio |
|---:|---|---:|---:|
| 1 | I1 S1 L0 R1 | 104.93 | 4.09% |
| 2 | I1 S1 L1 R1 | 106.00 | 5.16% |
| 3 | I1 S1 L0 R0 | 114.00 | 13.08% |
| 4 | I0 S1 L0 R1 | 114.07 | 13.13% |
| 5 | I1 S1 L1 R0 | 114.07 | 13.14% |
| 6 | I0 S1 L0 R0 | 114.73 | 13.81% |
| 7 | I0 S1 L1 R1 | 116.60 | 15.67% |
| 8 | I0 S1 L1 R0 | 119.60 | 18.63% |
| 9 | I1 S0 L0 R0 | 128.40 | 27.36% |
| 10 | I1 S0 L1 R0 | 132.80 | 31.70% |
| 11 | I1 S0 L1 R1 | 134.07 | 32.99% |
| 12 | I1 S0 L0 R1 | 136.87 | 35.73% |
| 13 | I0 S0 L0 R0 | 141.80 | 40.65% |
| 14 | I0 S0 L1 R0 | 144.13 | 42.95% |
| 15 | I0 S0 L0 R1 | 145.40 | 44.19% |
| 16 | I0 S0 L1 R1 | 150.20 | 48.98% |

## Efeitos principais

| Fator | Desligado | Ligado | Redução média |
|---|---:|---:|---:|
| Avaliação incremental | 130.82 | 121.39 | +9.42 |
| Substituição de serviço | 139.21 | 113.00 | +26.21 |
| Lambda local | 125.03 | 127.18 | -2.16 |
| Utilidade por arrependimento | 126.19 | 126.02 | +0.17 |

## Interações pareadas

| Par | Sinergia |
|---|---:|
| Avaliação incremental × Substituição de serviço | -5.85 |
| Avaliação incremental × Lambda local | +2.95 |
| Avaliação incremental × Utilidade por arrependimento | +3.35 |
| Substituição de serviço × Lambda local | +0.05 |
| Substituição de serviço × Utilidade por arrependimento | +10.05 |
| Lambda local × Utilidade por arrependimento | +1.52 |

## Melhor combinação por instância

Configuração vencedora agregada: **I1 S1 L0 R1**.

| Instância | Ótimo | Custo médio | GAP médio |
|---:|---:|---:|---:|
| 28 | 102 | 106.00 | 3.92% |
| 100 | 101 | 113.00 | 11.88% |
| 128 | 101 | 102.00 | 0.99% |
| 129 | 100 | 101.00 | 1.00% |
| 147 | 100 | 102.67 | 2.67% |

## Conclusão

- As quatro melhorias **não devem ser ligadas simultaneamente**: a melhor combinação foi I1 S1 L0 R1.
- A substituição composta apresentou o maior efeito principal, reduzindo o custo médio em 26,21 unidades.
- A utilidade por arrependimento teve efeito principal quase nulo, mas sua interação com a substituição composta gerou sinergia positiva de 10,05 unidades. Portanto, ela é útil dentro da combinação vencedora, não isoladamente.
- O lambda local piorou o custo médio em 2,16 unidades e foi desativado na configuração promovida.
- Como foram usadas três repetições e um orçamento curto, o resultado deve ser tratado como evidência experimental para seleção de configuração, não como uma prova de dominância em todas as sementes.
