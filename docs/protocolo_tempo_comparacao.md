# Comparação fiel ao protocolo original do artigo

## Protocolo de tempo

A reprodução usa 94 instâncias e 3 repetições por instância. O orçamento de cada repetição era originalmente relativo ao tempo do método exato registrado no log (tempo exato / 20 para ótimos < 2 s, tempo exato / 10 nos demais), mas esse protocolo relativo deixava a maioria das instâncias com menos de 0,2 s de orçamento, tempo insuficiente para a GLS convergir. O protocolo foi então fixado em:

- orçamento fixo de 10 s por repetição, independente do tempo do método exato;
- a execução encerra antecipadamente se encontra o ótimo conhecido.

A configuração C++ usada foi a GLS promovida `I1 S1 L0 R1`: avaliação incremental ligada, substituição composta ligada, lambda local desligado e utilidade por arrependimento ligada.

## Resultado reproduzido em 23/09/2026 (orçamento fixo de 10 s)

| Método | Execuções ótimas | Instâncias com pelo menos um ótimo | Custo médio |
|---|---:|---:|---:|
| Java original do repositório SSUU (`ArticleResult.java`, reexecutado com o mesmo orçamento fixo de 10 s) | N/D (não impresso pelo programa) | 0/94 | 155,51 (153,88 melhor por instância) |
| C++ GLS promovida | 279/282 | 93/94 | 100,63 |

O Java original não atingiu o ótimo em nenhuma das 94 instâncias mesmo com 10 s fixos por repetição (GAP médio 54,61%). O programa Java só imprime custo médio/melhor por instância, não a contagem de execuções individuais que bateram o ótimo, por isso essa coluna fica em branco para ele. Ambos os lados foram reexecutados nesta rodada (23/09/2026) sob o mesmo protocolo de 10 s fixos, então a comparação acima é direta.

## Resultado anterior (protocolo relativo /10, /20) — substituído

Com o orçamento relativo, a GLS atingia o ótimo em apenas 131/282 execuções (46/94 instâncias), custo médio 105,40, porque 86 de 94 instâncias recebiam menos de 0,55 s e 82 de 94 menos de 0,20 s (mediana 0,0605 s, mínimo 0,025 s de orçamento). O protocolo fixo de 10 s acima substitui essa reprodução.

## ILS#3 do artigo (Best Improvement) — reproduzido em 25/09/2026

O artigo original também descreve outras duas configurações do ILS (`ILS#2`, `ILS#3`) além da que o repositório dos autores efetivamente commitou (`ArticleResult.java` = ILS#1). Reproduzimos o **ILS#3** (Neighborhood Strategy = Best Improvement, Perturbation Movement = Move, Neighborhood Structure = Move, α = 0,2) também com orçamento fixo de 10 s por repetição.

Isso exigiu duas mudanças no repositório Java:
- `ILS.java`: o método `ILS()` rodava as 10.000 iterações sem checar o relógio (só o laço externo verificava, entre chamadas completas) — foi adicionada uma checagem de deadline dentro do laço, senão o Best Improvement (mais caro por avaliar todos os vizinhos que melhoram, não só o primeiro) poderia estourar o orçamento por muito tempo, como já tinha acontecido no lado C++ (ver `docs/avaliacao_grasp_adaptativo.md`, seção 5.2 — uma instância sozinha não terminava em 120s).
- `ArticleResult.java`: parametrizado via `-Dsso.ils=3` para trocar `α`, `ImprovementCondition` e `ImprovementMode` sem afetar a reprodução ILS#1 já existente.

| Método | Instâncias no ótimo | Custo médio | GAP médio | Tempo médio até melhor |
|---|---:|---:|---:|---:|
| Java ILS#1 (First Improvement, SWAP, α=0,4) | 0/94 | 155,51 | 54,61% | 10,44s |
| **Java ILS#3 (Best Improvement, MOVE, α=0,2)** | **86/94** | **104,13** | **3,51%** | **2,17s** |
| C++ GLS promovida | 93/94 | 100,63 | 0,04% | 0,26s |

O ILS#3 é dramaticamente melhor que o ILS#1 — chega perto da GLS, embora ainda fique atrás em 7 instâncias onde a GLS acerta e o ILS#3 não (destaque: instância 100, onde o ILS#3 piora bastante — GAP de 206,9% — possivelmente por ficar preso em uma região ruim do espaço de busca dentro do orçamento).

## Artefatos

- Tabela visual (Java ILS#1 × C++ GLS): `experiments/07_protocolo_artigo/charts/article_comparison/tabela_comparativa_protocolo_artigo_10s.png`
- Tabela visual (Java ILS#1 × Java ILS#3 × C++ GLS): `experiments/07_protocolo_artigo/charts/article_comparison/tabela_comparativa_ils1_ils3_gls_10s.png`
- CSV C++: `experiments/07_protocolo_artigo/results/gls_promoted_article_protocol.csv`
- Saída Java ILS#1: `experiments/00_baseline_java/results/java_results_raw.txt`
- Saída Java ILS#3: `experiments/00_baseline_java/results/java_ils3_10s_raw.txt`
- Script da tabela 3 vias: `experiments/07_protocolo_artigo/scripts/plot_ils3_comparison.py`

