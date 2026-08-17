# Path relinking sem deadline — comparação

## Resultado geral nas 94 instâncias

| Configuração | Ótimos | Custo médio dos melhores | GAP médio do melhor | GAP das médias | Tempo externo | Tempo interno |
|---|---:|---:|---:|---:|---:|---:|
| Java | 0/94 | 164,340 | 63,380% | 67,036% | 3:01,18 | — |
| Best-fit + oscilação sem GLS | 83/94 | 101,702 | 1,107% | 1,173% | 15:21,67 | 14:35,39 |
| GLS anterior sem deadline | **85/94** | **101,340** | **0,749%** | **0,829%** | 12:49,88 | 12:18,03 |
| GLS + path relinking sem deadline | 82/94 | 101,372 | 0,780% | 0,998% | **11:42,36** | **11:00,20** |

## Cinco instâncias difíceis — melhor custo de três execuções

| Instância | Ótimo | Java | GAP | Best-fit sem GLS | GAP | GLS anterior | GAP | Path relinking | GAP |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 100 | 101 | 210 | 107,9% | 141 | 39,6% | **117** | **15,8%** | 129 | 27,7% |
| 128 | 101 | 202 | 100,0% | 112 | 10,9% | 112 | 10,9% | **109** | **7,9%** |
| 129 | 100 | 191 | 91,0% | 109 | 9,0% | 106 | 6,0% | **104** | **4,0%** |
| 147 | 100 | 204 | 104,0% | 107 | 7,0% | **104** | **4,0%** | **104** | **4,0%** |
| 28 | 102 | 193 | 89,2% | 125 | 22,5% | 120 | 17,6% | **118** | **15,7%** |
| **Média das cinco** | — | **200,0** | **98,4%** | **118,8** | **17,8%** | **111,8** | **10,9%** | **112,8** | **11,9%** |

## Leitura do resultado

O path relinking foi muito competitivo: terminou 1min17,83s antes do GLS anterior no tempo interno e ficou apenas 0,031 ponto percentual atrás no GAP global do melhor. Contra esse GLS, venceu seis instâncias, empatou 84 e perdeu quatro.

Nas cinco difíceis, o path relinking venceu nas instâncias 128, 129 e 28, empatou na 147 e perdeu fortemente na 100. Essa única perda explica por que seu GAP médio nas difíceis ficou em 11,87%, contra 10,88% do GLS anterior.

Esta comparação não é um A/B pareado: os arquivos históricos foram executados com sementes e políticas temporais distintas. Para concluir superioridade estatística sem esse viés, seria necessário repetir GLS estrutural e path relinking sem deadline com a mesma `SSO_SEED`.
