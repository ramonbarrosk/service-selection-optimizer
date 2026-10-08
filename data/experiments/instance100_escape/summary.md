# Instância 100: diagnóstico do mínimo local da GLS

Programas: `experiments/diagnose_instance100.cpp` e `experiments/escape_instance100.cpp`
(compilar com as mesmas flags do Makefile + `-DENABLE_GLS`).

## Estrutura da instância

| |T| | |S| | Smax | Vres | consumo total | capacidade (Smax·Vres) | soma dos custos mínimos | ótimo |
|---|---|---|---|---|---|---|---|
| 100 | 100 | 39 | 14 | 536 | 546 (98,2%) | 100 | 101 |

Smax = 39 é exatamente o número mínimo de serviços que comporta o consumo,
e a folga total é de apenas 10 unidades. Na prática é um bin packing quase
saturado em que quase toda tarefa precisa ficar num serviço de custo 1.

## A GLS promovida é determinística

A solução inicial e a GLS não usam aleatoriedade, por isso as "10 repetições
independentes" de 30 s seguem a mesma trajetória: 104 em ~18 s e nenhuma
melhora até 120 s.

## Mais tempo e variações (1 execução por linha)

| estratégia | 60 s | 120 s | tempo até o melhor |
|---|---|---|---|
| promovida (alpha 0,3) | 104 | 104 | 19 s |
| alpha 0,03 | 106 | | 12 s |
| alpha 0,1 / 0,15 / 0,2 | 103 | 103 | 3–51 s |
| alpha 0,5 | | **102** | 15 s |
| alpha 0,7 / 1,5 | | 103 | 8–18 s |
| alpha 1,0 | **102** | **102** | 26 s |
| alpha 2,0 | | **102** | 17 s |
| alpha 3,0 | | 105 | 7 s |
| reinício + perturbação (alpha 0,3) | 105 | 105 | 1,5 s |
| reinício + perturbação (alpha 1,0) | | 102 | 35–44 s |
| oscilação estratégica na estagnação | 104 | | 19 s |

Todas as configurações estagnam antes de 51 s. Só aumentar o tempo não
chega ao ótimo; o alpha muda qual platô é atingido.

## Distância até o ótimo

| solução | custo | tarefas fora do custo mínimo | tarefas em serviço diferente do ótimo | serviços diferentes |
|---|---|---|---|---|
| GLS alpha 0,3 | 104 | 4 | 44 | 10 |
| GLS alpha 1,0 | 102 | 2 (t76, t84) | 20 | 6 |
| ótimo (CPLEX) | 101 | 1 (t76) | — | — |

## Vizinhanças maiores aplicadas à solução 102

| vizinhança | resultado |
|---|---|
| reempacotamento exato de k ≤ 4 serviços (+1 vazio) | sem melhora (ótimo local) |
| destruição/reparo exato, 6 serviços | 144 mil reparos provados sem melhora |
| destruição/reparo, 8–20 serviços (com limite de nós) | sem melhora |
| **oráculo:** reempacotar os 15 serviços que diferem do ótimo | **101 em < 0,01 s** |
| **oráculo:** destruir os 9 serviços certos (25 tarefas) | **101 em 0,01 s** |

Para sair do 102 é preciso um movimento coordenado: abrir 6 serviços vazios
(s18, s28, s29, s70, s90, s99) e fechar 6, puxando tarefas de vários serviços
ao mesmo tempo. Por exemplo, s28 recebe t4, t28, t37, t46 e t84, que vêm de 5
serviços diferentes. O reparo exato resolve esse movimento instantaneamente;
o difícil é escolher quais serviços destruir.
