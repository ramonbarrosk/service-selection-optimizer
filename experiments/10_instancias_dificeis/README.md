# Fase 10: instâncias difíceis geradas

## Motivação

Nas 94 instâncias do artigo, o ótimo do método exato é igual ao limite inferior trivial
(cada tarefa no seu serviço mais barato) em 92 casos. Com custos uniformes em [1, 20] e
100 serviços, cada tarefa tem em média 5 serviços de custo 1, e quase sempre dá para
usá-los sem violar as restrições. As duas exceções, as instâncias 28 e 100, são as de
capacidade quase saturada: a soma dos consumos ocupa cerca de 97% de `Smax × Vres`.

## Gerador

`tools/generate_instance.cpp` (`make build/generate-instance`) é o porte do gerador
original (`gen-inst/main.cpp` do projeto de referência) com semente explícita. Sem opções,
gera exatamente o mesmo arquivo que o original geraria com a mesma semente. As opções:

| Opção | Padrão | Efeito |
|---|---|---|
| `--seed`, `--out` | obrigatórias | Semente do `mt19937` e arquivo de saída |
| `--tasks`, `--services` | 100, 100 | Tamanho |
| `--cost-max`, `--resource-max`, `--prob-max` | 20, 10, 0,4 | Limites das distribuições |
| `--tightness t` | desligado | Fixa `Vres` para que soma dos consumos / (`Smax × Vres`) ≈ t |
| `--corr rho` | 0 | Correlaciona custo e confiabilidade (os serviços com menor probabilidade de violação ficam mais caros) |
| `--check-feasible` | desligado | Descarta a instância (código de saída 2) se a construção inicial não achar solução viável |

## Conjunto

`scripts/generate_instances.sh` gera 15 instâncias por perfil em `data/instances_hard`,
com as sementes testadas em ordem a partir de 1. Entram as primeiras viáveis, sem olhar o
desempenho dos métodos. O manifesto `results/manifest.csv` guarda a semente e os
parâmetros de cada instância.

| Perfil | Ids | Parâmetros |
|---|---|---|
| T97, T98, T99, T100 | 1101–1115, 1201–1215, 1301–1315, 1401–1415 | `--tightness` 0,97 a 1,00 |
| C50 | 1501–1515 | `--corr 0.5`, `Vres` original |

## Ótimo das instâncias

`scripts/run_exato.sh` roda o método exato do projeto de referência: o executável
`services` (MIP com CPLEX linkado estaticamente, limite de 15 min, 1 thread). Os logs com
`Status = Optimal` vão para `data/Log_hard`, no formato de `data/Log`. Os que param pelo
limite de tempo vão para `data/Log_hard_timeout` e valem só como limite superior.

Os dois solvers leem `data/Log_hard` (`SSO_INSTANCE_DIR`/`SSO_LOG_DIR` no C++,
`-Dsso.dir`/`-Dsso.logdir` no Java). Sem log, usam o limite inferior trivial como
referência, e alcançá-lo também prova o ótimo. A análise usa, nesta ordem: o ótimo do
exato, o limite inferior alcançado ou a melhor solução conhecida.

## Execução

```bash
bash experiments/10_instancias_dificeis/scripts/generate_instances.sh
bash experiments/10_instancias_dificeis/scripts/run_exato.sh        # JOBS=3, até 15 min por instância
bash experiments/10_instancias_dificeis/scripts/run_comparacao.sh   # REPETITIONS=3 TIME_SECONDS=10 (protocolo da fase 07)
python3 experiments/10_instancias_dificeis/scripts/analyze_comparacao.py
python3 experiments/10_instancias_dificeis/scripts/plot_comparacao.py     # tabelas e gráficos em charts/
```

O `run_comparacao.sh` exige o projeto Java recompilado com as propriedades `sso.dir` e
`sso.logdir` em `ArticleResult.java`.
