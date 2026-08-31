# Experimento inicial (antes da otimização): probabilidade na função objetivo da GLS

## Objetivo experimental

A variante permite que a trajetória visite soluções que excedem a SLA, usando:

`h_prob(s) = custo(s) + lambda * penalidades_GLS(s) + mu * max(0, P_violacao(s) - Pmax)`

Capacidade e `Smax` continuam sendo restrições duras. A melhor solução e a resposta final são sempre viáveis e comparadas pelo custo real.

## Protocolo

- Instâncias: 94
- Repetições por instância: 3
- Orçamento por variante e repetição: 0.550 s
- Peso probabilístico inicial (`beta`): 0.05
- Mesma solução inicial para as duas variantes em cada repetição.
- GLS/GFLS estrita: SLA tratada como restrição dura.
- GLS/GFLS suave: excesso da SLA incluído no objetivo aumentado.

## Resultado agregado

| Variante | Custo médio | GAP médio | Tempo médio | Execuções ótimas | Instâncias ótimas |
|---|---:|---:|---:|---:|---:|
| GLS estrita | 102.57 | 1.97% | 0.550 s | 193/282 | 71/94 |
| GLS com SLA suave | 105.72 | 5.10% | 0.550 s | 112/282 | 47/94 |

## Travessias da região inviável

- Comparação pareada por custo médio: 0 vitórias, 29 empates e 65 derrotas da variante suave.
- Movimentos aceitos enquanto a SLA estava violada: 6.
- Retornos à região viável: 4.
- Travessias ocorreram em 1/282 execuções e 1/94 instâncias.
- Respostas finais inviáveis: 0.

## Conclusão

A implementação comprova que a GLS pode atravessar temporariamente a fronteira probabilística e recuperar uma solução viável. Neste conjunto, porém, isso foi raro e não gerou vitória. Sob o mesmo orçamento, o cálculo probabilístico adicional reduziu o trabalho de busca que coube no tempo e piorou o resultado agregado. Portanto, esta variante deve permanecer experimental e não substituir a GLS estrita na configuração padrão.
