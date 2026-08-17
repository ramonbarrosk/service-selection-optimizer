# GLS estrutural com Path Relinking

## Implementação

O ILS mantém um conjunto elite com até cinco soluções. Uma nova solução entra no conjunto somente quando difere em pelo menos cinco atribuições de uma elite próxima, preservando diversidade.

Após uma chamada GLS sem melhoria e com GAP conhecido de pelo menos 5%:

1. a incumbente é usada como solução inicial;
2. a elite mais distante é escolhida como guia;
3. cada passo aplica o MOVE viável de menor custo que aproxima uma tarefa da guia;
4. quando útil, um SWAP direcionado aproxima duas tarefas simultaneamente;
5. todas as soluções intermediárias são avaliadas;
6. a melhor intermediária recebe a busca local e a oscilação já existentes;
7. todo o processo respeita o deadline.

## Triagem inicial — cinco difíceis

| Configuração | GAP do melhor | GAP das médias | Vitórias/empates/derrotas |
|---|---:|---:|---:|
| GLS estrutural | **10,88%** | 14,57% | — |
| GLS + path relinking | 11,86% | **14,04%** | 3/0/2 |

A maioria de vitórias e a melhora das médias justificaram a validação completa.

## Validação inicial nas 94 — sementes aleatórias

| Configuração | Ótimos | GAP do melhor | GAP das médias |
|---|---:|---:|---:|
| GLS estrutural | **63/94** | 1,499% | 2,087% |
| Path relinking sem filtro | 58/94 | 1,383% | 1,971% |
| Path relinking condicionado a GAP >= 5% | 59/94 | **1,350%** | **1,967%** |

O filtro protege instâncias próximas do ótimo e concentra o método nas difíceis. Como essas execuções ainda começavam com `random_device`, foi realizado um A/B final pareado.

## A/B pareado nas 94 — semente 20260813

| Configuração | Ótimos | GAP do melhor | GAP das médias | Tempo externo |
|---|---:|---:|---:|---:|
| GLS estrutural | 58/94 | 1,626% | 2,176% | 3:10,03 |
| GLS + path relinking condicionado | **59/94** | **1,531%** | **2,155%** | 3:12,02 |

Comparando o melhor custo por instância, o path relinking teve 8 vitórias, 83 empates e 3 derrotas. Comparando a média das três repetições, foram 11 vitórias, 78 empates e 5 derrotas.

## Conclusão

O ganho é moderado, mas consistente no teste pareado: mais um ótimo, redução de 0,095 ponto percentual no GAP do melhor e duas vezes mais vitórias que derrotas. Entre as extensões avaliadas — nova utilidade, cadeia de realocação, reconstrução parcial e path relinking — esta é a primeira a melhorar simultaneamente ótimos e GAP contra seu baseline com sementes iguais.

Execução reproduzível:

```bash
SSO_SEED=20260813 make run-gls-path-relinking
```

O gatilho de GAP usa o ótimo conhecido e é adequado ao experimento. Para uso em instâncias sem ótimo conhecido, ele deve ser substituído por um limite inferior ou por um critério de estagnação/tempo restante.
