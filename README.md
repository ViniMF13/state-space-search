# Bridge and Torch: comparação de algoritmos de busca

Trabalho Prático I de Fundamentos de Inteligência Artificial (UFMG).
Compara **DFS, BFS, Custo Uniforme (UCS) e A\*** no quebra-cabeça da Ponte e
da Tocha, com infraestrutura de medição limpa.

## Estrutura

```
include/   instance.hpp  problem instances (classic, random, geometric)
           state.hpp     bitmask state, successor edges, heuristics
           metrics.hpp   POD SearchMetrics (expanded, generated, frontier...)
           search.hpp    uniform graph_search for all 4 algorithms
src/       C++17 sources
analysis/  plot.py       figures + tables from CSV results
report/    main.tex      LaTeX skeleton (fill prose per enunciado)
build/     CMake build dir (ignored)
```

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Release/O2 é obrigatório.

## Uso

```bash
./build/tp1_app --selftest      # valida corretude (ótimo=17, h admissível, A*(h0)==UCS)
./build/tp1_app --classic       # instância {1,2,5,10}, cap 15000 -> results_classic.csv
./build/tp1_app --benchmark     # n=4..12, famílias random+geometric -> results_benchmark.csv
```

Flags: `--trials 30`, `--cap N`, `--ns 4,6,8,10,12`, `--instances 5`, `--out FILE`.

## Análise

```bash
python3 -m venv .venv
.venv/bin/pip install -r analysis/requirements.txt
.venv/bin/python analysis/plot.py    # -> analysis/out/*.png|pdf + tabelas
```

Saídas: `fig1_expansoes` (esforço vs n), `fig2_tempo`, `fig3_tempo_vs_expansoes`
(custo ~1-3 µs/nó, tempo ∝ expansões), `fig4_qualidade` (desvio do ótimo),
`fig5_fronteira` (memória), `fig6_heuristicas` + `fig7_heuristicas_geometricas`
(h0..h3), `table_classic`.

## Design da medição 

- Algoritmos só incrementam um `struct SearchMetrics` POD (`++m.expanded`):
  sem locks, sem alocação, sem I/O no caminho crítico.
- Tempo medido com `steady_clock` ao redor da chamada pura; 2 aquecimentos +
  30 execuções cronometradas; mediana reportada.
- Cada solução é re-verificada por replay independente do caminho
  (`recheck_path`), além de checagens de determinismo no harness.
- O mesmo laço de busca gera BFS/DFS/UCS/A* (política de fronteira via
  prioridade + número de sequência): comparação justa entre métodos.
- CSV por execução é a fonte da verdade; `analysis/plot.py` valida
  (determinismo, otimalidade de UCS/A*(h1,h2)) antes de plotar.

## Heurísticas

- `h0 = 0` — A* degenera em UCS (checagem interna).
- `h1 = max` tempo restante — admissível (toda pessoa restante cruza ≥ 1 vez
  com custo ≥ c_i).
- `h2 =` soma dos máximos dos pares dos tempos restantes (mais forte,
  admissível: relaxação em que a tocha volta de graça).
- `h3 =` soma dos tempos restantes — **não admissível**, incluído para
  demonstrar perda de garantia de otimalidade vs. menor esforço.
