| model                          |       size |     params | backend    | threads |            test |                  t/s |
| ------------------------------ | ---------: | ---------: | ---------- | ------: | --------------: | -------------------: |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       2 |           pp128 |         42.45 ± 0.21 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       2 |            tg64 |         20.13 ± 3.10 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       4 |           pp128 |         37.64 ± 1.74 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       4 |            tg64 |         18.33 ± 1.28 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       6 |           pp128 |         44.63 ± 0.07 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       6 |            tg64 |         19.54 ± 0.34 |

build: 92d6c8a (1)
