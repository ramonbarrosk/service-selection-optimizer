# Histórico dos experimentos

Cada pasta corresponde a uma fase do TCC, em ordem cronológica. Dentro de cada fase:

- `scripts/`: scripts de execução, tabelas e gráficos;
- `results/`: saídas brutas (`.txt`, `.csv`, `.log`) e resumos (`.md`);
- `charts/`: figuras geradas a partir dos resultados.

Os scripts devem ser executados a partir da raiz do repositório.

## Fases

| Fase | Período | Pergunta | Conclusão |
|---|---|---|---|
| [`00_baseline_java`](00_baseline_java) | mai–set/2026 | Saídas do ILS Java do repositório de referência. | Base de comparação usada pelas demais fases. |
| [`01_reproducao_ils_cpp`](01_reproducao_ils_cpp) | mai/2026 | O ILS reimplementado em C++ reproduz o Java? | Primeira comparação Java × C++. |
| [`02_hibrido_best_fit_oscilacao`](02_hibrido_best_fit_oscilacao) | jul/2026 | Construção best-fit e oscilação estratégica melhoram o ILS? | O híbrido levou o GAP médio do ILS#1 de 52% para 4,1%. |
| [`03_fls_e_dive`](03_fls_e_dive) | ago/2026 | Fast Local Search e mergulho por soluções inviáveis ajudam? | A FLS foi praticamente neutra (47 × 46 ótimos). O mergulho piorou o custo médio nas 5 instâncias difíceis (161,4 × 117,2). |
| [`04_ils_vs_gls`](04_ils_vs_gls) | ago/2026 | A GLS isolada supera o ILS? | Sim: GAP médio de 36,66% (ILS) para 1,85% (GLS) em 0,55 s. A oscilação não ajudou. |
| [`05_gls_sla_flexivel`](05_gls_sla_flexivel) | ago/2026 | Tratar a SLA como penalidade na função objetivo ajuda a GLS? | O ganho foi de apenas 0,06 ponto percentual de GAP. Não foi adotado, e a SLA continua como restrição rígida. |
| [`06_gls_melhorias`](06_gls_melhorias) | set/2026 | Quais melhorias tornam a GLS mais forte? | Fatorial 2⁴ que definiu a variante promovida `I1 S1 L0 R1`. |
| [`07_protocolo_artigo`](07_protocolo_artigo) | set/2026 | Como a GLS se compara ao Java no protocolo do artigo? | Com 10 s fixos: GLS 93/94 ótimos, Java ILS#1 0/94, ILS#3 86/94. |
| [`08_ablacao_gls`](08_ablacao_gls) | out/2026 | Qual a contribuição de cada melhoria nas 94 instâncias? | Ablação completa, varredura de α e diagnóstico da instância 100. |

## O que ainda roda na versão atual

Somente as fases **06, 07 e 08** usam o código atual da GLS. O experimento C++ fica em
`06_gls_melhorias/compare_gls_improvements.cpp` e é compilado com
`make experiment-gls-improvements`. Os alvos `make run-gls-*` reproduzem os experimentos
da fase 06, e `make run-gls-ablation` reproduz a ablação da fase 08.

Na limpeza, a variante `osc_cond` (GLS + oscilação condicional) saiu desse experimento.
Ela aparece apenas nos resultados históricos de `gls_improvements_difficult5.csv`.

As fases **00 a 05** dependem de código que foi removido: o ILS, a busca genérica com
oscilação e FLS, a GLS com SLA flexível e os programas auxiliares de cada fase. Os scripts
de execução dessas fases estão marcados como arquivados. Os resultados e gráficos continuam
aqui como registro. Para executar qualquer um deles, use a tag que preserva o código
completo:

```bash
git checkout archive/pre-cleanup
```

Na tag, os arquivos estão nos caminhos originais (`scripts/`, `data/`, `experiments/`).
O mesmo vale para os programas `diagnose_instance100.cpp` e `escape_instance100.cpp`
citados em `08_ablacao_gls/results/instance100_escape/summary.md`.

## Observação sobre `java_results_raw.txt`

O arquivo `00_baseline_java/results/java_results_raw.txt` foi sobrescrito ao longo do
trabalho e hoje contém a rodada Java com 10 s fixos (23/09/2026). Algumas tabelas
anteriores, como `07_protocolo_artigo/results/article_protocol_comparison.md`, foram
geradas com uma versão anterior desse arquivo. Por isso, reexecutar os scripts que as
geram produz números diferentes dos versionados. A versão anterior do arquivo pode ser
recuperada no histórico do git.
