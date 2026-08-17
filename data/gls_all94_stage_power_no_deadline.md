# Poder de cada etapa — 94 instâncias sem deadline

Configuração: best-fit + busca local/FLS + oscilação + ILS + GLS/GFLS, com
três execuções por instância. Cada execução completou as 10.000 iterações
normais (ou 2.000 nas instâncias configuradas como rápidas), sem interrupção
por relógio dentro do ILS ou do GLS.

- Tempo externo: **12min49,88s**
- Tempo interno informado pelo programa: **12min18,03s**
- Ótimo em pelo menos uma execução: **85/94**
- Média das três execuções exatamente ótima: **84/94**
- Custo médio final: **101,53**
- GAP médio do custo final: **0,93%**

## Contribuição observada

| Medida | Média |
|---|---:|
| Ótimo conhecido | 100,59 |
| Best-fit | 111,37 |
| Após busca local inicial | 110,71 |
| Após oscilação inicial | 108,82 |
| Ganho local/FLS total | -0,69 |
| Ganho da oscilação total | -8,70 |
| Ganho direto GLS/GFLS | -0,45 |
| Custo final | 101,53 |

Participação nos novos recordes:

- busca local/FLS: **7,0%**;
- oscilação: **88,4%**;
- GLS/GFLS direto: **4,6%**.

O GLS estabeleceu ganho direto em 35 das 94 instâncias. O valor ainda não inclui
ganhos indiretos, isto é, melhorias posteriores obtidas depois que o GLS mudou a
região da trajetória.

## Deadline versus execução completa

| Medida | Deadline adaptativo | Sem deadline | Diferença |
|---|---:|---:|---:|
| Tempo externo | 3min44,50s | 12min49,88s | **3,43x** |
| Custo final médio | 102,53 | **101,53** | -1,00 |
| GAP médio | 1,93% | **0,93%** | -1,00 p.p. |
| Ótimo em ao menos uma execução | 62/94 | **85/94** | +23 |
| Média das três no ótimo | 47/94 | **84/94** | +37 |
| Chamadas GLS médias | 1,05 | **18,99** | +17,94 |
| Instâncias com ganho GLS direto | 5/94 | **35/94** | +30 |
| Ganho GLS direto médio | -0,13 | **-0,45** | -0,32 |

Comparando o custo final médio por instância, a execução sem deadline venceu em
44 instâncias, empatou em 47 e perdeu em 3. Como o algoritmo é estocástico e o
consumo da sequência aleatória muda quando os deadlines interrompem trajetórias,
isso não constitui uma comparação pareada perfeita.

## Cinco instâncias difíceis

| Instância | Com deadline | Sem deadline | GAP sem deadline |
|---:|---:|---:|---:|
| 100 | 122,33 | 122,67 | 21,45% |
| 129 | 107,67 | **106,33** | 6,33% |
| 147 | 106,67 | **106,00** | 6,00% |
| 28 | 121,33 | 123,67 | 21,24% |
| 128 | 118,67 | **113,67** | 12,54% |

O tempo adicional melhorou três das cinco difíceis e piorou ligeiramente duas.
Isso é compatível com variabilidade estocástica: mais tempo melhora fortemente o
resultado agregado, mas não garante monotonicidade entre processos independentes.
