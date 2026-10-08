# Experimento: probabilidade na função objetivo da GLS

## Objetivo experimental

A variante permite que a trajetória visite soluções que excedem a SLA, usando:

`h_prob(s) = custo(s) + lambda * penalidades_GLS(s) + mu * max(0, P_violacao(s) - Pmax)`

Capacidade e `Smax` continuam sendo restrições duras. A melhor solução e a resposta final são sempre viáveis e comparadas pelo custo real.

## Protocolo

- Instâncias: 7
- Repetições por instância: 10
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
| GLS estrita otimizada | 116.74 | 15.73% | 0.550 s | 278446759 | 5949.0 | 12/70 | 2/7 |
| GLS com SLA suave | 116.69 | 15.68% | 0.550 s | 277885423 | 5946.5 | 13/70 | 2/7 |
| GLS com SLA suave condicional | 116.71 | 15.71% | 0.550 s | 276936911 | 5909.6 | 12/70 | 2/7 |

## Travessias da região inviável

- Suave x estrita (V/E/D): 1/6/0 por instância; 2/67/1 por execução.
- Condicional x estrita (V/E/D): 1/6/0 por instância; 1/69/0 por execução.
- Suave: 279 movimentos inviáveis, 155 retornos, 18/70 execuções com travessia.
- Condicional: 25 movimentos inviáveis, 17 retornos, 6/70 execuções com travessia.
- Respostas finais inviáveis (suave/condicional): 0/0.
- Deriva probabilística máxima final (suave/condicional): 0.000e+00/0.000e+00.

## Conclusão

A implementação otimizada reduziu o GAP da versão sempre suave em 0.057 ponto percentual frente à estrita. A variante condicional alterou o GAP em 0.029 ponto percentual. A busca sempre preservou um incumbente viável. Como o ganho da versão suave ainda está concentrado em poucas instâncias, ela apresenta retorno experimental, mas requer mais repetições antes de substituir a configuração padrão.
