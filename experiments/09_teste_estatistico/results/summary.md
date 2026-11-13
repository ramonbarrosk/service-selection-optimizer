# Teste de Wilcoxon: ILS#3 x GLS nas instâncias difíceis

Instâncias: 15; repetições por método: 20; orçamento de 10 s por repetição, com parada ao atingir o ótimo.

Um único teste de Wilcoxon pareado (postos sinalizados), exato e bilateral, sobre o custo médio por instância: cada instância é um par (GLS, ILS#3), e a hipótese nula é que a diferença entre os métodos é simétrica em torno de zero. Instâncias empatadas são descartadas.

**Resultado:** n = 12 instâncias com diferença (empates: 42, 60, 67), W+ = 78, W- = 0, p = 4,9e-4. Com α = 0,05, há diferença estatisticamente significativa.

W+ soma os postos das instâncias em que o ILS#3 teve custo maior (pior) que a GLS.

| Instância | GLS | ILS#3 |
|---:|---:|---:|
| 5 | 100,00 | 105,30 |
| 11 | 100,00 | 108,55 |
| 21 | 101,00 | 101,05 |
| 24 | 102,00 | 102,40 |
| 28 | 102,00 | 174,95 |
| 40 | 101,00 | 103,35 |
| 42 | 100,00 | 100,00 |
| 51 | 100,00 | 100,10 |
| 60 | 100,00 | 100,00 |
| 67 | 100,00 | 100,00 |
| 100 | 105,00 | 376,70 |
| 101 | 100,00 | 109,40 |
| 128 | 101,00 | 137,90 |
| 129 | 100,00 | 112,60 |
| 147 | 100,00 | 129,45 |

GLS e ILS#3: custo médio das repetições.
