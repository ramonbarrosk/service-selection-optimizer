# GLS estrutural — reformulação e validação

## Mudança aplicada

O GLS original escolhia uma atribuição tarefa-serviço para penalizar usando somente:

`utilidade = custo_atribuicao / (1 + penalidade)`

A variante estrutural mantém a função objetivo guiada original, mas seleciona as features com:

`custo_estrutural = custo_atribuicao + 2 * custo_minimo_medio * pressao_capacidade * (1 + peso_tarefa) + 0,5 * arrependimento`

`utilidade_estrutural = custo_estrutural / (1 + penalidade)`

Onde:

- `pressao_capacidade` é o uso do serviço dividido por `Vres`;
- `peso_tarefa` é o consumo da tarefa dividido por `Vres`;
- `arrependimento` é a diferença entre o custo atual da tarefa e seu menor custo possível.

A função usada para aceitar movimentos continua sendo:

`f_GLS(s) = custo_real(s) + lambda * soma_das_penalidades_ativas(s)`

Assim, a mudança não inventa outra restrição suave nem duplica a oscilação: ela tenta direcionar o GLS para atribuições caras que também estejam ligadas ao gargalo de capacidade.

## Triagem isolada — cinco difíceis

Cada configuração teve três execuções e o mesmo orçamento (`SSO_TIME_SCALE=0.80`).

| Configuração | GAP médio das médias | GAP médio do melhor | Tempo externo |
|---|---:|---:|---:|
| Sem GLS | 16,17% | 12,47% | 2:13,67 |
| GLS atual, alpha=0,3 | **14,26%** | 12,28% | 2:13,82 |
| GLS estrutural, alpha=1,0 | 14,57% | **10,88%** | 2:12,25 |
| GLS estrutural, alpha=2,0 | 16,04% | 13,66% | 2:13,94 |

O GLS estrutural com `alpha=1,0` venceu o GLS atual em três instâncias, empatou uma e perdeu uma considerando o melhor custo. `alpha=2,0` penalizou agressivamente demais e foi descartado.

## Validação completa — 94 instâncias e orçamento de 3 minutos

| Configuração | Ótimos | GAP médio do melhor | GAP médio das médias | Tempo externo |
|---|---:|---:|---:|---:|
| Best-fit + oscilação, sem GLS | 60/94 | 1,731% | 2,358% | 3:07,34 |
| GLS atual | 59/94 | 1,510% | **2,067%** | 3:07,42 |
| GLS estrutural | **63/94** | **1,499%** | 2,087% | 3:06,77 |

Contra o GLS atual, o estrutural teve 16 vitórias, 63 empates e 15 derrotas. Contra a solução sem GLS, foram 19 vitórias, 60 empates e 15 derrotas.

Nas cinco difíceis dentro da execução completa, o GAP do melhor foi 16,82% sem GLS, 13,26% com o GLS atual e 13,47% com o estrutural. Portanto, o ganho da triagem isolada não se repetiu uniformemente.

## Conclusão

A reformulação melhora a contagem de ótimos em quatro sobre o GLS atual e três sobre a solução sem GLS. Entretanto, comparada ao GLS atual, reduz o GAP global em apenas 0,011 ponto percentual e piora o GAP das médias em 0,020 ponto. Isso é um sinal útil, mas ainda não justifica promovê-la como configuração padrão.

Por isso, os pesos estruturais permanecem desligados por padrão. A variante fica disponível, reproduzível e separada por flags/targets para novas rodadas com sementes independentes:

```bash
make build-gls-structural
make run-gls-structural
```

O próximo avanço com chance de justificar o GLS deve mudar sua vizinhança, e não apenas calibrar penalidades: por exemplo, usar cadeias de realocação/ejection chains guiadas pelas penalidades para atravessar barreiras que MOVE e SWAP isolados não vencem.
