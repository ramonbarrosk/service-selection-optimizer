# Curva de tempo — GLS/GFLS promovida

- Instâncias: 28, 100, 128, 129, 147
- Repetições por orçamento: 1
- Orçamentos: 0.55s, 2s, 5s, 10s, 30s
- Configuração: incremental=1, substituição=1, lambda local=0, arrependimento=1.

| Instância | Ótimo | Custo em 0.55s | Custo em 2s | Custo em 5s | Custo em 10s | Custo em 30s |
|---:|---:|---:|---:|---:|---:|---:|
| 28 | 102 | 106.00 | 103.00 | 102.00 | 102.00 | 102.00 |
| 100 | 101 | 113.00 | 105.00 | 105.00 | 105.00 | 104.00 |
| 128 | 101 | 102.00 | 102.00 | 101.00 | 101.00 | 101.00 |
| 129 | 100 | 100.00 | 100.00 | 100.00 | 100.00 | 100.00 |
| 147 | 100 | 102.00 | 100.00 | 100.00 | 100.00 | 100.00 |

## GAP médio

- 0.55s: 3.76%
- 2s: 1.19%
- 5s: 0.79%
- 10s: 0.79%
- 30s: 0.59%

## Resultado

- Instâncias que atingiram o ótimo no maior orçamento: 28, 128, 129, 147.
- O tempo até a melhor solução está disponível no CSV para distinguir melhoria tardia de platô.
