# Experimento: probabilidade na função objetivo da GLS

## Objetivo experimental

A variante permite que a trajetória visite soluções que excedem a SLA, usando:

`h_prob(s) = custo(s) + lambda * penalidades_GLS(s) + mu * max(0, P_violacao(s) - Pmax)`

Capacidade e `Smax` continuam sendo restrições duras. A melhor solução e a resposta final são sempre viáveis e comparadas pelo custo real.

## Protocolo

- Instâncias: 1
- Repetições por instância: 20
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
| GLS estrita otimizada | 101.35 | 1.35% | 0.550 s | 277355277 | 5383.4 | 3/20 | 1/1 |
| GLS com SLA suave | 101.00 | 1.00% | 0.550 s | 277333268 | 5376.2 | 5/20 | 1/1 |
| GLS com SLA suave condicional | 101.35 | 1.35% | 0.550 s | 277042320 | 5377.4 | 3/20 | 1/1 |

## Travessias da região inviável

- Suave x estrita (V/E/D): 1/0/0 por instância; 4/15/1 por execução.
- Condicional x estrita (V/E/D): 0/1/0 por instância; 0/20/0 por execução.
- Suave: 108 movimentos inviáveis, 47 retornos, 13/20 execuções com travessia.
- Condicional: 0 movimentos inviáveis, 0 retornos, 0/20 execuções com travessia.
- Respostas finais inviáveis (suave/condicional): 0/0.
- Deriva probabilística máxima final (suave/condicional): 0.000e+00/0.000e+00.

## Conclusão

A implementação otimizada reduziu o GAP da versão sempre suave em 0.350 ponto percentual frente à estrita. A variante condicional alterou o GAP em 0.000 ponto percentual. A busca sempre preservou um incumbente viável. Como o ganho da versão suave ainda está concentrado em poucas instâncias, ela apresenta retorno experimental, mas requer mais repetições antes de substituir a configuração padrão.
