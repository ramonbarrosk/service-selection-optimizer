# Service Selection Optimizer

Projeto do Trabalho de Conclusão de Curso (TCC) de **Ramon Barros de Lima**.

O trabalho aborda o problema de **seleção de serviços sob incerteza** e propõe uma
**Busca Local Guiada (GLS)** em C++ para resolvê-lo. Os resultados são comparados com o
ILS (*Iterated Local Search*) do projeto de referência
[Service-Selection-Under-Uncertainty](https://github.com/dimascnf/Service-Selection-Under-Uncertainty),
implementado em Java pelos autores do artigo original, usando as mesmas 94 instâncias
e os ótimos conhecidos do método exato.

## O problema

Cada instância tem 100 tarefas e 100 serviços candidatos. Cada tarefa deve ser alocada
a exatamente um serviço, e o objetivo é minimizar o custo total da alocação, respeitando:

- **Capacidade:** a soma do consumo das tarefas alocadas a um serviço não pode passar de `Vres`;
- **Serviços empregados:** no máximo `Smax` serviços podem ser usados;
- **SLA probabilístico:** cada serviço tem uma probabilidade de violar o SLA, e a
  probabilidade de mais de `Vmax` tarefas sofrerem violação não pode passar de `Pmax`.

## O algoritmo

A solução final é a GLS com GFLS (*Guided Fast Local Search*) e três melhorias que se
mostraram vantajosas nos experimentos (variante `I1 S1 L0 R1`):

| Melhoria | O que faz |
|---|---|
| **Avaliação incremental** | Atualiza a distribuição de probabilidade do SLA em O(Vmax) a cada movimento, em vez de revalidar a solução inteira. |
| **Substituição composta** | Nova vizinhança que troca um serviço empregado por um vazio, transferindo todas as suas tarefas de uma vez. Isso permite atravessar a barreira de `Smax`, que MOVE e SWAP isolados não atravessam. |
| **Utilidade por arrependimento** | A utilidade de penalizar uma atribuição usa o custo acima do menor custo possível da tarefa, em vez do custo bruto. |

A busca parte de uma solução inicial construída de forma determinística: cada tarefa vai
para o serviço de menor probabilidade de violação que mantém a solução viável. A partir
daí, a GLS faz descidas de melhor melhoria nas vizinhanças MOVE, SWAP e substituição
composta, minimizando `custo + λ · penalidades`. As restrições continuam rígidas: as
penalidades só guiam a trajetória, e o resultado é sempre a melhor solução viável pelo
custo real.

Os parâmetros fixos são `α = 0,3` (escala do λ) e 30 rodadas de penalização por ciclo,
com as penalidades preservadas entre ciclos.

## Resultados

Protocolo: 94 instâncias, 3 repetições por instância, orçamento fixo de 10 s por
repetição, com parada antecipada ao alcançar o ótimo conhecido. Os dois lados foram
executados sob o mesmo protocolo.

| Método | Instâncias com ótimo | Custo médio | GAP médio |
|---|---:|---:|---:|
| Java ILS#1 (configuração publicada no repositório de referência) | 0/94 | 155,51 | 54,61% |
| Java ILS#3 (Best Improvement, descrito no artigo) | 86/94 | 104,13 | 3,51% |
| **C++ GLS (este trabalho)** | **93/94** | **100,63** | **0,04%** |

A GLS alcançou o ótimo em 279 das 282 execuções. Detalhes em
[`docs/protocolo_tempo_comparacao.md`](docs/protocolo_tempo_comparacao.md) e no relatório
[`docs/relatorio_gls_ils_ablacao.pdf`](docs/relatorio_gls_ils_ablacao.pdf).

## Como executar

Requisitos: `g++` com suporte a C++17 e `make`. Os scripts de análise usam Python 3 com
`matplotlib` e `numpy`. O comando abaixo compila e executa a GLS em todas as instâncias
com o protocolo acima (até cerca de 47 minutos, se nenhuma repetição parar antes):

```bash
make run
```

Variáveis de ambiente opcionais:

| Variável | Padrão | Descrição |
|---|---|---|
| `SSO_INSTANCES` | todas | Lista de instâncias, por exemplo `1,28,100` |
| `SSO_REPETITIONS` | `3` | Repetições por instância |
| `SSO_TIME_SECONDS` | `10` | Orçamento de cada repetição, em segundos |

Exemplo rápido:

```bash
make && SSO_INSTANCES=1,28,100 SSO_TIME_SECONDS=1 ./build/service-selection-optimizer
```

A saída traz uma linha por instância (ótimo, tempo do método exato, custo médio, melhor
custo, tempo médio até a melhor solução e quantas repetições alcançaram o ótimo) e um
resumo ao final.

## Estrutura do repositório

```
src/
  main.cpp                   execução da GLS no protocolo de comparação
  search/
    GuidedLocalSearcher.hpp  GLS/GFLS e as três melhorias
    GlsSolver.hpp            laço de ciclos da GLS com orçamento de tempo
    InitialSolution.hpp      construção da solução inicial
  instance/                  modelo e leitura das instâncias
  basic/                     alocação, tarefa e serviço
  validator/                 verificação de viabilidade (capacidade, Smax, SLA)
data/
  instances/                 as 94 instâncias do artigo de referência
  Log/                       logs do método exato, com o custo ótimo de cada instância
experiments/                 histórico dos experimentos, organizado por fase
docs/                        relatórios e análises
```

## Histórico dos experimentos

A pasta [`experiments/`](experiments/README.md) guarda os scripts, resultados e gráficos
de todas as fases do trabalho: a reprodução do ILS, o híbrido com best-fit e oscilação,
a comparação ILS × GLS, as melhorias da GLS e a ablação final.

O código das abordagens descartadas (ILS, best-fit, oscilação estratégica, FLS e GLS com
SLA flexível) foi removido da versão atual, mas continua preservado na tag
`archive/pre-cleanup`:

```bash
git checkout archive/pre-cleanup
```
