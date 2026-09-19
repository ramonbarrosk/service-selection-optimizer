# Curva de tempo — GLS/GFLS promovida

- Instâncias: 28, 100, 128, 129, 147
- Repetições por orçamento: 1
- Orçamentos: 0.55s, 2s, 5s, 10s, 30s
- Configuração: avaliação incremental, substituição composta de serviço, lambda local e utilidade por arrependimento.

| Instância | Ótimo | Custo em 0.55s | Custo em 2s | Custo em 5s | Custo em 10s | Custo em 30s |
|---:|---:|---:|---:|---:|---:|---:|
| 28 | 102 | 107.00 | 105.00 | 103.00 | 102.00 | 102.00 |
| 100 | 101 | 109.00 | 107.00 | 107.00 | 104.00 | 104.00 |
| 128 | 101 | 106.00 | 103.00 | 103.00 | 103.00 | 102.00 |
| 129 | 100 | 103.00 | 100.00 | 100.00 | 100.00 | 100.00 |
| 147 | 100 | 106.00 | 100.00 | 100.00 | 100.00 | 100.00 |

## GAP médio

- 0.55s: 5.35%
- 2s: 2.17%
- 5s: 1.78%
- 10s: 0.99%
- 30s: 0.79%

## Resultado

- Instâncias que atingiram o ótimo no maior orçamento: 28, 129, 147.
- O tempo até a melhor solução está disponível no CSV para distinguir melhoria tardia de platô.
