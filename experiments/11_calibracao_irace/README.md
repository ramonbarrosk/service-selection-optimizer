# Fase 11: calibração da GLS com o irace

## Motivação

Os parâmetros da GLS (`alpha = 0,3`, 30 rodadas por ciclo e as opções de
`GuidedLocalSearchOptions`) foram escolhidos à mão e pelo fatorial da fase 06. Esta fase
os calibra com o [irace](https://mlopez-ibanez.github.io/irace/) (corridas iteradas com
teste de Friedman), em instâncias separadas das de teste, e valida a configuração
obtida contra a atual.

## Parâmetros

| Parâmetro | Variável | Faixa | Atual |
|---|---|---|---|
| `alpha`: λ = α · custo inicial / tarefas | `SSO_GLS_ALPHA` | [0,01; 1], escala log | 0,3 |
| `rounds`: rodadas de penalização por ciclo | `SSO_GLS_ROUNDS` | 5 a 100 | 30 |
| `gfls`: busca local rápida guiada (ativação por vizinhança) | `SSO_GLS_GFLS` | 0/1 | 1 |
| `replacement`: vizinhança de troca de serviço | `SSO_GLS_REPLACEMENT` | 0/1 | 1 |
| `regret`: utilidade com arrependimento | `SSO_GLS_REGRET` | 0/1 | 1 |
| `calibrate`: λ calculado após a primeira descida | `SSO_GLS_CALIBRATE` | 0/1 | 0 |
| `maxpen`: características penalizadas por rodada (0 = todos os empates) | `SSO_GLS_MAX_PENALIZED` | 0 a 3 | 0 |

As variáveis valem para `build/service-selection-optimizer` e `build/gls-run`
(`glsParametersFromEnv` em `src/search/GlsSolver.hpp`). Sem elas, a GLS roda na
configuração atual. As opções que só mudam a velocidade (avaliação incremental, intervalos
de reconstrução e de checagem do relógio) ficam fora da calibração.

## Protocolo

- **Treino:** 50 instâncias novas em `data/instances_train`, 10 por perfil da fase 10
  (T97, T98, T99, T100, C50), com sementes a partir de 10001. O conjunto de teste usa
  sementes até 33, então nenhuma instância de teste entra na calibração. As 94 originais
  ficam de fora do treino porque quase toda configuração alcança o ótimo nelas, e o irace
  não teria o que diferenciar.
- **Execução avaliada:** `tools/gls_run.cpp`, uma execução de 10 s com orçamento inteiro,
  sem parada no ótimo. O valor minimizado é o GAP para o limite inferior trivial, que põe
  instâncias de custos diferentes na mesma escala.
- **irace:** 3.000 execuções, 3 em paralelo (cerca de 3 h), `deterministic = 1` porque a
  GLS não usa números aleatórios. A configuração atual entra como configuração inicial
  (`irace/configurations.txt`).
- **Validação:** a configuração de elite contra a atual, nas 94 instâncias do artigo e
  nas 75 da fase 10, no protocolo da fase 07 (3 × 10 s, parando no ótimo conhecido), com o
  Wilcoxon pareado da fase 09.

## Execução

Requer R com o pacote irace:

```bash
sudo apt install -y r-base r-base-dev
Rscript -e 'd <- Sys.getenv("R_LIBS_USER"); dir.create(d, recursive=TRUE, showWarnings=FALSE); install.packages("irace", lib=d, repos="https://cloud.r-project.org")'
```

A partir da raiz do repositório:

```bash
bash experiments/11_calibracao_irace/scripts/generate_training.sh   # 50 instâncias de treino
bash experiments/11_calibracao_irace/scripts/run_irace.sh           # results/best_configurations.csv
bash experiments/11_calibracao_irace/scripts/run_validacao.sh       # results/validacao/*.csv
python3 experiments/11_calibracao_irace/scripts/analyze_validacao.py
```

## Resultados (29/10/2026)

O irace fez 6 iterações em 3 h 08 min. A melhor configuração (342) foi `alpha = 0,997`,
`rounds = 79` e `maxpen = 3`, com `gfls`, `replacement` e `regret` ligados e `calibrate`
desligado, como na configuração atual. O `alpha` ficou no limite superior da faixa.
As outras elites estão em `results/best_configurations.csv`.

Validação (`results/validacao/summary.md`), 3 × 10 s:

| Conjunto | Config. | Ótimo alcançado | GAP médio | Melhor que a outra | p-valor |
|---|---|---:|---:|---:|---:|
| 94 do artigo | atual | 93/94 | 0,04% | 0 | 1,000 |
| 94 do artigo | calibrada | 93/94 | 0,01% | 1 | |
| 75 difíceis | atual | 46/75 | 6,64% | 12 | 0,160 |
| 75 difíceis | calibrada | 51/75 | 6,44% | 19 | |

A configuração calibrada alcança o ótimo em mais 5 instâncias difíceis (todas nos perfis
T98 a T100) e melhora a melhor solução conhecida em 5 das 7 instâncias C50 em que o
CPLEX não provou o ótimo. A diferença de custo não é significativa (p = 0,16). Nas
instâncias difíceis, o GAP usa o limite inferior trivial quando não há ótimo provado.
