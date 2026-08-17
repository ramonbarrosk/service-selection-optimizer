# Java × best-fit × GLS/GFLS — política temporal alinhada

Foram executadas 94 instâncias, três vezes cada, usando a configuração ILS#1:

- `alpha = 0,4`;
- busca local `first-improvement`;
- perturbação `MOVE`;
- vizinhança `SWAP`;
- 10.000 iterações, ou 2.000 nas instâncias rápidas;
- orçamento verificado somente entre chamadas completas do ILS.

No C++, essa política é ativada por `SSO_JAVA_BUDGET_MODE=1`. O ILS e o GLS
recebem deadline infinito; após cada chamada completa, o executor verifica se
o orçamento da repetição terminou, reproduzindo o `do-while` do Java.

## Resultado das 94 instâncias

| Solução | Média dos melhores custos | GAP médio dos melhores | Atingiu ótimo | Ótimo nas 3 repetições | Tempo externo |
|---|---:|---:|---:|---:|---:|
| Java original | 164,34 | 63,38% | 0/94 | não disponível¹ | 3min01,18s |
| Best-fit + FLS + oscilação | 101,35 | 0,76% | 84/94 | 78/94 | 12min06,63s |
| Best-fit + FLS + oscilação + GLS/GFLS | **101,27** | **0,68%** | **85/94** | **83/94** | 13min18,32s |

¹ O agregador Java original não reinicia `bestCost` entre as repetições. Seu
melhor global é válido, mas a contagem por repetição e a média impressa não são
independentes. O agregador C++ foi corrigido antes destes novos runs.

Comparando o melhor resultado por instância, o GLS venceu o best-fit em 5,
empatou em 86 e perdeu em 3. Comparando a média correta das três repetições C++,
o GLS venceu em 13, empatou em 79 e perdeu em 2.

O GLS custou 1min11,69s adicionais, aumentou a contagem de ótimo em pelo menos
uma execução de 84 para 85 e a consistência de ótimo nas três de 78 para 83.

## Cinco instâncias mais difíceis

| Instância | Ótimo | Java melhor | GAP | Best-fit melhor | GAP | GLS melhor | GAP |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 100 | 101 | 210 | 107,9% | 122 | 20,8% | **116** | **14,9%** |
| 129 | 100 | 191 | 91,0% | **106** | **6,0%** | 107 | 7,0% |
| 147 | 100 | 204 | 104,0% | **105** | **5,0%** | **105** | **5,0%** |
| 28 | 102 | 193 | 89,2% | 117 | 14,7% | **114** | **11,8%** |
| 128 | 101 | 202 | 100,0% | **111** | **9,9%** | 113 | 11,9% |
| **Média** | — | **200,0** | **98,4%** | **112,2** | **11,3%** | **111,0** | **10,1%** |

Nas difíceis, o GLS venceu duas, empatou uma e perdeu duas. Mesmo assim, reduziu
o custo médio dos melhores de 112,2 para 111,0 e o GAP médio de 11,3% para 10,1%.
Nenhuma das três abordagens atingiu o ótimo nessas cinco.

## Ressalva sobre tempo

Agora a **política de orçamento** é igual, mas o tempo de parede não precisa ser.
A regra permite que uma chamada já iniciada termine mesmo após o orçamento. Uma
chamada C++ com oscilação e GLS custa mais que uma chamada Java original; por isso
os executáveis C++ ultrapassam mais o orçamento e terminam em aproximadamente
12–13 minutos, enquanto o Java termina em 3 minutos.

Para comparar qualidade por exatamente o mesmo tempo de CPU, seria necessário um
experimento adicional com deadline interno igual para os três programas.
