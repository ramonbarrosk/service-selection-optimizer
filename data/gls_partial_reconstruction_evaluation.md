# GLS com reconstrução parcial guiada

## Abordagem implementada

A reconstrução parcial é uma busca de grande vizinhança (*destroy-and-repair*):

1. ordena as tarefas pela combinação de penalidade GLS e custo atual;
2. remove as três atribuições mais críticas;
3. testa conjuntamente até `10^3` reinserções;
4. poda combinações que já não podem superar a incumbente;
5. verifica capacidade durante a construção e valida `Smax` e SLA na solução completa;
6. interrompe imediatamente se o deadline for atingido.

Foram testadas duas funções de aceitação:

- **guiada:** minimiza `custo + lambda * penalidades`;
- **custo real:** as penalidades escolhem o grupo destruído, mas o repair só aceita custo real menor.

## Triagem nas cinco instâncias difíceis

Três repetições, mesmo orçamento interno de aproximadamente 2min08s por configuração.

| Configuração | Melhor médio | GAP do melhor | GAP das médias | Tempo externo |
|---|---:|---:|---:|---:|
| GLS estrutural sem reconstrução | **111,8** | **10,88%** | **14,57%** | 2:12,25 |
| Repair guiado, grupo 3, período 5 | 114,0 | 13,07% | 15,70% | 2:16,60 |
| Repair guiado, grupo 3, período 30 | 114,2 | 13,26% | 15,77% | 2:16,86 |
| Repair por custo real, grupo 3, período 5 | 113,2 | 12,28% | 14,85% | 2:16,28 |

A melhor reconstrução foi a versão conservadora por custo real. Contra o GLS estrutural, teve uma vitória, um empate e três derrotas.

## Conclusão

A reconstrução funciona corretamente e respeita o deadline, mas não justificou seu custo. Torná-la menos frequente não resolveu o problema, indicando que a limitação principal não era apenas tempo. Otimizar custo real evitou parte da degradação causada pela função guiada, porém ainda ficou 1,40 ponto percentual pior no GAP do melhor.

Por isso, a opção permanece experimental e desligada por padrão:

```bash
make run-gls-partial-reconstruction
```

Não foi executada nas 94 instâncias porque falhou de forma consistente no filtro das cinco difíceis. O executável padrão foi restaurado para o GLS estrutural sem reconstrução.
