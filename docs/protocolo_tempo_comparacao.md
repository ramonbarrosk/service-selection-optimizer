# Comparação fiel ao protocolo original do artigo

## Protocolo de tempo

A reprodução usa 94 instâncias e 3 repetições por instância. O orçamento de cada repetição é relativo ao tempo do método exato registrado no log:

- instância com tempo exato menor que 2 s: orçamento = tempo exato / 20;
- instância com tempo exato maior ou igual a 2 s: orçamento = tempo exato / 10;
- não existe piso de 0,55 s;
- a execução encerra antecipadamente se encontra o ótimo conhecido.

A configuração C++ usada foi a GLS promovida `I1 S1 L0 R1`: avaliação incremental ligada, substituição composta ligada, lambda local desligado e utilidade por arrependimento ligada.

## Resultado reproduzido em 20/09/2026

| Método | Execuções ótimas | Instâncias com pelo menos um ótimo | Custo médio |
|---|---:|---:|---:|
| Java original do repositório SSUU | 0/282 | 0/94 | 163,53 (melhor por instância) |
| C++ GLS promovida | 131/282 | 46/94 | 105,40 |

O Java original não atingiu o ótimo em nenhuma das 282 execuções desta reprodução. Esse número é o resultado desta execução específica; não é uma propriedade fixa do artigo, pois o algoritmo é estocástico.

## Por que o resultado é menor que o teste de 0,55 s

O protocolo relativo é muito mais restritivo nas instâncias rápidas:

- 86 de 94 instâncias receberam menos de 0,55 s;
- 82 de 94 receberam menos de 0,20 s;
- a mediana do orçamento foi 0,0605 s;
- o menor orçamento foi 0,025 s.

Por isso, o resultado de 46/94 no protocolo original não deve ser comparado diretamente com os 89/94 obtidos usando 0,55 s fixos. Os 89/94 pertencem a um protocolo experimental diferente.

## Artefatos

- Tabela visual: `data/charts_article_comparison/tabela_comparativa_protocolo_original_artigo.png`
- CSV C++: `data/experiments/gls_promoted_article_protocol.csv`
- Saída Java: `data/java_results_raw.txt`

