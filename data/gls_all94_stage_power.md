# Poder de cada etapa — todas as 94 instâncias

Configuração vencedora: best-fit + busca local/FLS + oscilação + ILS +
GLS/GFLS. Foram realizadas três execuções por instância com deadlines
adaptativos. Tempo externo total: **3min44,50s**.

## Resultado agregado

| Medida | Valor médio |
|---|---:|
| Ótimo conhecido | 100,59 |
| Custo do best-fit | 111,37 |
| Após busca local inicial | 110,70 |
| Após oscilação inicial | 108,82 |
| Ganho local total | -0,70 |
| Ganho da oscilação total | -8,01 |
| Ganho direto do GLS/GFLS | -0,13 |
| Custo final | 102,53 |
| GAP final | 1,93% |

A redução média total foi de **8,84 pontos**, levando o custo de `111,37` para
`102,53`. A participação nos novos recordes foi:

- **Oscilação:** 90,6% da redução observada; produziu ganho em 76/94 instâncias.
- **Busca local/FLS:** 7,9%; produziu ganho em 24/94 instâncias.
- **GLS/GFLS direto:** 1,5%; produziu ganho direto em 5/94 instâncias.

Em qualidade final:

- 62/94 instâncias atingiram o ótimo em pelo menos uma das três execuções;
- 47/94 tiveram custo médio das três execuções igual ao ótimo;
- 83/94 terminaram com GAP médio de até 5%;
- 11/94 terminaram com GAP médio acima de 5%.

## Como interpretar a atribuição

Os checkpoints `best-fit`, `após busca local inicial` e `após oscilação inicial`
representam a aplicação inicial de cada mecanismo. Os ganhos totais contabilizam
somente reduções que estabeleceram um novo recorde global durante a trajetória.

O ganho direto do GLS é deliberadamente conservador. Se o GLS desloca a solução
para outra região e uma oscilação posterior encontra um novo recorde, a redução é
creditada à oscilação. Assim, o relatório mostra **quem efetuou a redução**, não o
efeito causal completo de remover o componente. Para medir o efeito causal, deve-se
usar uma ablação com e sem GLS em orçamentos de tempo iguais.

## Instâncias com maior GAP médio

| Instância | GAP médio |
|---:|---:|
| 100 | 21,12% |
| 28 | 18,95% |
| 128 | 17,49% |
| 11 | 8,33% |
| 129 | 7,67% |
| 40 | 7,26% |
| 147 | 6,67% |
| 101 | 6,33% |
| 146 | 6,00% |
| 5 | 5,67% |

Os valores das cinco difíceis podem variar em relação ao teste isolado porque a
busca é estocástica e, na execução completa, elas recebem outro estado da sequência
aleatória depois do processamento das instâncias anteriores.
