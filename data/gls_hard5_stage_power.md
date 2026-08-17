# Poder de cada etapa — cinco instâncias mais difíceis

Configuração: best-fit + FLS + oscilação + ILS + GLS/GFLS, com três execuções e
os mesmos deadlines adaptativos. Para cada repetição foi considerada a trajetória
que produziu o menor custo final.

| Instância | Ótimo | Best-fit | Após local inicial | Após OSC inicial | Ganho local total | Ganho OSC total | Ganho GLS direto | Final médio |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 100 | 101 | 182,0 | 176,0 | 161,0 | -9,0 | -39,3 | -7,7 | 126,0 |
| 129 | 100 | 159,0 | 148,7 | 146,0 | -10,3 | -39,3 | 0,0 | 109,3 |
| 147 | 100 | 140,0 | 140,0 | 135,0 | 0,0 | -32,7 | 0,0 | 107,3 |
| 28 | 102 | 170,0 | 158,0 | 155,0 | -12,7 | -36,3 | -2,7 | 118,3 |
| 128 | 101 | 147,0 | 144,0 | 136,0 | -3,0 | -27,0 | -0,3 | 116,7 |
| **Média** | **100,8** | **159,6** | **153,3** | **146,6** | **-7,0** | **-34,9** | **-2,1** | **115,5** |

Os checkpoints `Após local inicial` e `Após OSC inicial` mostram a primeira
aplicação de cada mecanismo. Já os ganhos totais incluem somente reduções que
estabeleceram um novo recorde global durante toda a trajetória:

- **Ganho local total:** busca local inicial, busca local nas iterações do ILS e
  polimento local após chamadas GLS.
- **Ganho OSC total:** oscilação inicial, oscilação nas iterações do ILS e
  polimento por oscilação após chamadas GLS.
- **Ganho GLS direto:** novo recorde produzido diretamente pela descida guiada,
  antes do polimento local e da oscilação.

Os ganhos são aditivos: `Best-fit - ganho local - ganho OSC - ganho GLS = final`.
Por exemplo, na instância 100: `182 - 9 - 39,3 - 7,7 = 126`.

## Ressalva de interpretação

O ganho direto do GLS é uma medida conservadora. Se o GLS muda a solução para
outra região e, depois, a oscilação encontra um novo recorde, o ganho é creditado
à oscilação, que efetuou a redução final. Portanto, esta instrumentação mede
**quem estabeleceu o recorde**, não o efeito causal completo de retirar cada
componente. A ablação anterior continua sendo a evidência apropriada para medir
o efeito total da remoção de um mecanismo.
