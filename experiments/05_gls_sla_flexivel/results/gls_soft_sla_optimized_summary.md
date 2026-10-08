# Experimento: probabilidade na função objetivo da GLS

## Objetivo experimental

A variante permite que a trajetória visite soluções que excedem a SLA, usando:

`h_prob(s) = custo(s) + lambda * penalidades_GLS(s) + mu * max(0, P_violacao(s) - Pmax)`

Capacidade e `Smax` continuam sendo restrições duras. A melhor solução e a resposta final são sempre viáveis e comparadas pelo custo real.

## Protocolo

- Instâncias: 94
- Repetições por instância: 3
- Orçamento por variante e repetição: 0.550 s
- Peso probabilístico inicial (`beta`): 0.05
- Cauda suave da variante condicional: 20% do tempo.
- Proximidade exigida: margem de até 25% de `Pmax`.
- Mesma solução inicial para as três variantes em cada repetição.
- GLS/GFLS estrita: SLA tratada como restrição dura.
- GLS/GFLS suave: excesso da SLA incluído no objetivo aumentado.
- GLS/GFLS condicional: SLA suave somente na cauda do tempo e perto de `Pmax`.

## Resultado agregado

| Variante | Custo médio | GAP médio | Tempo | Candidatos | Rodadas | Execuções ótimas | Instâncias ótimas |
|---|---:|---:|---:|---:|---:|---:|---:|
| GLS estrita otimizada | 102.47 | 1.87% | 0.550 s | 287320265 | 5358.2 | 208/282 | 76/94 |
| GLS com SLA suave | 102.44 | 1.84% | 0.550 s | 286728910 | 5343.1 | 211/282 | 77/94 |
| GLS com SLA suave condicional | 102.46 | 1.86% | 0.550 s | 286282451 | 5335.5 | 209/282 | 76/94 |

## Travessias da região inviável

- Suave x estrita (V/E/D): 3/91/0 por instância; 4/278/0 por execução.
- Condicional x estrita (V/E/D): 1/93/0 por instância; 1/281/0 por execução.
- Suave: 109 movimentos inviáveis, 48 retornos, 6/282 execuções com travessia.
- Condicional: 1 movimentos inviáveis, 1 retornos, 1/282 execuções com travessia.
- Respostas finais inviáveis (suave/condicional): 0/0.
- Deriva probabilística máxima final (suave/condicional): 0.000e+00/0.000e+00.

## Conclusão

A implementação otimizada reduziu o GAP da versão sempre suave em 0.025 ponto percentual frente à estrita. A variante condicional alterou o GAP em 0.004 ponto percentual. A busca sempre preservou um incumbente viável. Como o ganho da versão suave ainda está concentrado em poucas instâncias, ela apresenta retorno experimental, mas requer mais repetições antes de substituir a configuração padrão.
