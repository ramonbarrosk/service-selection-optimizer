# Curva adaptativa das cinco instâncias restantes

- Configuração: I1 S1 L0 R1.
- Uma repetição por orçamento.
- Primeira etapa: 2s, 5s, 10s e 30s nas cinco instâncias.
- Segunda etapa: 60s e 120s somente na instância 100.
- Tempo interno adicional do experimento: 253.29s.

| Instância | Ótimo | 0.55s | 2s | 5s | 10s | 30s | 60s | 120s |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 11 | 100 | 102.00 | 100.00 | 100.00 | 100.00 | 100.00 | 100.00 | 100.00 |
| 28 | 102 | 106.00 | 103.00 | 102.00 | 102.00 | 102.00 | 102.00 | 102.00 |
| 100 | 101 | 112.67 | 105.00 | 105.00 | 105.00 | 104.00 | 104.00 | 104.00 |
| 128 | 101 | 102.00 | 102.00 | 101.00 | 101.00 | 101.00 | 101.00 | 101.00 |
| 147 | 100 | 102.00 | 100.00 | 100.00 | 100.00 | 100.00 | 100.00 | 100.00 |

## GAP médio

- 0.55s: 4.09%
- 2s: 1.19%
- 5s: 0.79%
- 10s: 0.79%
- 30s: 0.59%
- 60s: 0.59%
- 120s: 0.59%

## Conclusão

- As instâncias 11 e 147 atingiram o ótimo com até 2 segundos.
- As instâncias 28 e 128 atingiram o ótimo com até 5 segundos.
- A instância 100 melhorou para custo 104 em 30 segundos, mas permaneceu em 104 também com 60 e 120 segundos; o ótimo conhecido é 101.
- Portanto, aumentar apenas o tempo zerou quatro dos cinco GAPs, mas não superou o platô estrutural da instância 100.
