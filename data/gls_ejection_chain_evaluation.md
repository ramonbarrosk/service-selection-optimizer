# Avaliação de cadeias de realocação no GLS

## Oportunidade testada

Foi adicionada ao GLS uma cadeia curta de duas realocações:

1. a tarefa `A` tenta entrar no serviço `B`, mas não cabe por capacidade;
2. uma tarefa `B` é expulsa desse serviço para um terceiro serviço `C`;
3. a solução final é aceita somente se respeitar capacidade, `Smax` e SLA;
4. a cadeia precisa reduzir a mesma função guiada do GLS:

`f_GLS(s) = custo_real(s) + lambda * soma_das_penalidades_ativas(s)`

Isso alcança soluções que um MOVE isolado não consegue produzir. Ao contrário de um SWAP, o destino da tarefa expulsa é um terceiro serviço.

## Três formulações avaliadas nas cinco difíceis

Todas tiveram três repetições e orçamento interno total de aproximadamente 2min08s.

| Formulação | GAP do melhor | GAP das médias | Diagnóstico |
|---|---:|---:|---|
| GLS estrutural sem cadeia | **10,88%** | 14,57% | Referência |
| Cadeia examinada sempre | 12,67% | 15,44% | Duplicava trabalho de MOVE/SWAP |
| Cadeia somente para barreira de capacidade | 12,47% | 14,65% | Mais barata, mas ainda examinada cedo demais |
| Cadeia como escape do mínimo MOVE/SWAP | 11,70% | **14,00%** | Melhor formulação da cadeia |

A versão de escape venceu o GLS estrutural em duas instâncias e perdeu em três considerando o melhor custo. Ela foi levada para a validação completa porque melhorou a média das três repetições.

## Validação nas 94 instâncias — mesmo orçamento de 3 minutos

| Configuração | Ótimos | GAP do melhor | GAP das médias | Tempo externo |
|---|---:|---:|---:|---:|
| Best-fit + oscilação, sem GLS | 60/94 | 1,731% | 2,358% | 3:07,34 |
| GLS atual | 59/94 | 1,510% | 2,067% | 3:07,42 |
| GLS estrutural | **63/94** | **1,499%** | 2,087% | 3:06,77 |
| GLS estrutural + cadeia de escape | 59/94 | 1,500% | **2,053%** | 3:04,83 |

Comparações pelo melhor custo de cada instância:

- cadeia contra GLS atual: 17 vitórias, 63 empates e 14 derrotas;
- cadeia contra GLS estrutural: 13 vitórias, 66 empates e 15 derrotas.

## Conclusão

A nova vizinhança funciona e melhora ligeiramente a robustez, mas ainda não justifica seu uso padrão. Comparada ao GLS estrutural, a cadeia reduz o GAP das médias em apenas 0,034 ponto percentual, mantém praticamente o mesmo GAP do melhor e perde quatro soluções ótimas.

Por isso, `GLS_EJECTION_CHAIN=0` continua sendo o padrão. A implementação permanece disponível para reprodução:

```bash
make run-gls-ejection-chain
```

O resultado sugere que aumentar novamente a profundidade da cadeia provavelmente terá retorno ruim sob deadline. Uma próxima oportunidade mais promissora seria uma reconstrução parcial guiada pelas penalidades: remover um pequeno grupo de atribuições muito penalizadas e reinseri-las em conjunto, executada somente após estagnação longa.
