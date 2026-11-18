# Fase 14 — ILS#3 em C++

Referência: 94 instâncias com ótimo provado; geradas: mesma referência do capítulo (fase 13), acrescida das soluções da ILS em C++ quando melhores.

## Referência, 10 s

| Método | Ótimo em ≥1 exec. | Execuções no ótimo (%) | Custo médio | Gap (%) | t (s) | Inviáveis |
|---|---:|---:|---:|---:|---:|---:|
| GLS | 93/94 | 98,9 | 100,60 | 0,01 | 0,25 | 0 |
| ILS Java | 87/94 | 80,6 | 105,77 | 5,14 | 3,13 | 0 |
| ILS C++ | 87/94 | 91,2 | 103,81 | 3,19 | 1,34 | 0 |

- ILS C++ × ILS Java: melhor 76 / igual 18 / pior 0; Wilcoxon n=76, W+=0, W-=2926, p=2,6e-23
- GLS × ILS C++: melhor 10 / igual 84 / pior 0; Wilcoxon n=10, W+=0, W-=55, p=0,002
- GLS × ILS Java: melhor 76 / igual 18 / pior 0; Wilcoxon n=76, W+=0, W-=2926, p=2,6e-23

## Referência, 0,55 s

| Método | Ótimo em ≥1 exec. | Execuções no ótimo (%) | Custo médio | Gap (%) | t (s) | Inviáveis |
|---|---:|---:|---:|---:|---:|---:|
| GLS | 90/94 | 95,7 | 100,83 | 0,25 | 0,10 | 0 |
| ILS Java | 20/94 | 3,6 | 121,20 | 20,46 | 1,07 | 0 |
| ILS C++ | 82/94 | 39,1 | 109,66 | 8,99 | 0,61 | 0 |

- ILS C++ × ILS Java: melhor 94 / igual 0 / pior 0; Wilcoxon n=94, W+=0, W-=4465, p=1,0e-28
- GLS × ILS C++: melhor 93 / igual 1 / pior 0; Wilcoxon n=93, W+=0, W-=4371, p=2,0e-28
- GLS × ILS Java: melhor 94 / igual 0 / pior 0; Wilcoxon n=94, W+=0, W-=4465, p=1,0e-28

- ILS C++ 10 s × ILS Java 10 s (idem acima); GLS 0,55 s × ILS C++ 10 s: melhor 10 / igual 84 / pior 0; Wilcoxon n=10, W+=0, W-=55, p=0,002

Geradas: referências alteradas pela ILS C++: nenhuma

## Geradas, 10 s

| Método | Ótimo em ≥1 exec. | Execuções no ótimo (%) | Custo médio | Gap (%) | t (s) | Inviáveis |
|---|---:|---:|---:|---:|---:|---:|
| GLS | 51/68 | 68,0 | 132,53 | 0,53 | 3,88 | 0 |
| ILS Java | 10/68 | 2,5 | 161,07 | 25,99 | 10,09 | 2 |
| ILS C++ | 12/68 | 5,7 | 151,57 | 17,27 | 9,79 | 4 |

- ILS C++ × ILS Java: melhor 73 / igual 0 / pior 2; Wilcoxon n=75, W+=11.5, W-=2838.5, p=3,1e-21
- GLS × ILS C++: melhor 75 / igual 0 / pior 0; Wilcoxon n=75, W+=0, W-=2850, p=5,3e-23
- GLS × ILS Java: melhor 75 / igual 0 / pior 0; Wilcoxon n=75, W+=0, W-=2850, p=5,3e-23

| Perfil | GLS gap | ILS Java gap | ILS C++ gap | C++ melhor/igual/pior que Java | GLS melhor/igual/pior que C++ |
|---|---:|---:|---:|---:|---:|
| T97 | 0,07 | 15,56 | 10,14 | 15/0/0 | 15/0/0 |
| T98 | 0,33 | 31,26 | 20,37 | 15/0/0 | 15/0/0 |
| T99 | 0,33 | 31,73 | 21,46 | 15/0/0 | 15/0/0 |
| T100 | 0,40 | 40,63 | 26,29 | 15/0/0 | 15/0/0 |
| C50 | 1,51 | 10,75 | 8,08 | 13/0/2 | 15/0/0 |

Tempo médio até o ótimo nas 18 instâncias de referência resolvidas em todas as execuções pelas duas versões (10 s): Java 1,16 s, C++ 0,45 s (razão 2,6).
