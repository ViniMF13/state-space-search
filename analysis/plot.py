#!/usr/bin/env python3
"""Generates figures and tables for the report from the benchmark CSVs."""

import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd

ROOT = Path(__file__).resolve().parent.parent
OUT = Path(__file__).resolve().parent / "out"
OUT.mkdir(exist_ok=True)

LABELS = {
    ("BFS", "h0"): "BFS",
    ("DFS", "h0"): "DFS",
    ("UCS", "h0"): "Custo Uniforme",
    ("ASTAR", "h0"): "A* (h0)",
    ("ASTAR", "h1"): "A* (h1)",
    ("ASTAR", "h2"): "A* (h2)",
    ("ASTAR", "h3"): "A* (h3)",
}
ORDER = list(LABELS.keys())
COLORS = plt.cm.tab10.colors
STYLES = {k: {"color": COLORS[i % 10], "marker": ["o", "s", "^", "v", "D", "P", "X"][i % 7]}
          for i, k in enumerate(ORDER)}

XL = "n (número de pessoas)"


def load() -> pd.DataFrame:
    classic = pd.read_csv(ROOT / "results_classic.csv")
    bench = pd.read_csv(ROOT / "results_benchmark.csv")
    classic["source"] = "classic"
    bench["source"] = "benchmark"
    df = pd.concat([classic, bench], ignore_index=True)
    df["series"] = list(zip(df.algo, df.heuristic))
    df["gap"] = df.solution_cost - df.optimal_cost
    return df


def validate(df: pd.DataFrame) -> None:
    bad_recheck = df[df.found == 1].path_recheck_ok.eq(0).sum()
    assert bad_recheck == 0, f"{bad_recheck} paths failed recheck"

    determinism = df.groupby(["source", "family", "n", "instance_idx", "algo", "heuristic"],
                             sort=False).expanded.nunique()
    assert (determinism == 1).all(), "non-deterministic expansion counts detected"
    print("validação: todos os paths OK; contagens de expansão determinísticas OK")

    opt = df[(df.solution_cost >= 0)]
    for a, h in [("UCS", "h0"), ("ASTAR", "h1"), ("ASTAR", "h2")]:
        sub = opt[(opt.algo == a) & (opt.heuristic == h)]
        assert (sub.gap == 0).all(), f"{a}/{h} found suboptimal solution!"
    print("validação: UCS e A*(h1,h2) sempre ótimos OK")

    h3 = opt[(opt.algo == "ASTAR") & (opt.heuristic == "h3")]
    n_sub = (h3.gap > 0).sum()
    n_tot = len(h3.drop_duplicates(["family", "n", "instance_idx"]))
    print(f"validação: A*(h3) subótimo em {n_sub} busca(s) de {n_tot} instâncias (sem garantia)")

    bfs = opt[opt.algo == "BFS"].drop_duplicates(["family", "n", "instance_idx"])
    print(f"validação: BFS subótimo em {(bfs.gap > 0).sum()}/{len(bfs)} instâncias")


def series_line(ax, data: pd.DataFrame, ycol: str, agg: str) -> None:
    for key in ORDER:
        sub = data[data.series == key]
        if sub.empty:
            continue
        g = sub.groupby("n")[ycol].agg(agg).reset_index()
        ax.plot(g.n, g[ycol], label=LABELS[key], linewidth=1.6, markersize=5, **STYLES[key])
    ax.set_xlabel(XL)
    ax.set_yscale("log")
    ax.grid(True, which="both", alpha=0.3)


def fig_expansions(df: pd.DataFrame) -> None:
    d = df[(df.source == "benchmark")]
    fig, ax = plt.subplots(figsize=(7, 4.4))
    series_line(ax, d, "expanded", "median")
    ax.set_ylabel("Nós expandidos (mediana, escala log)")
    ax.set_title("Esforço de busca vs tamanho do problema")
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(OUT / "fig1_expansoes.png", dpi=200)
    fig.savefig(OUT / "fig1_expansoes.pdf")
    plt.close(fig)


def fig_time(df: pd.DataFrame) -> None:
    d = df[df.source == "benchmark"]
    fig, ax = plt.subplots(figsize=(7, 4.4))
    series_line(ax, d, "time_ns", "median")
    ax.set_ylabel("Tempo mediano (ns, escala log)")
    ax.set_title("Tempo de processamento vs tamanho do problema")
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(OUT / "fig2_tempo.png", dpi=200)
    fig.savefig(OUT / "fig2_tempo.pdf")
    plt.close(fig)


def fig_time_vs_expansions(df: pd.DataFrame) -> None:
    d = df[(df.source == "benchmark") & (df.expanded > 0)].copy()
    fig, ax = plt.subplots(figsize=(7, 4.4))
    for key in ORDER:
        sub = d[d.series == key]
        if sub.empty:
            continue
        sample = sub.sample(min(len(sub), 400), random_state=0)
        ax.scatter(sample.expanded, sample.time_ns, s=8, alpha=0.5, label=LABELS[key],
                   **STYLES[key])
        coef = sub.expanded.cov(sub.time_ns) / sub.expanded.var()
        inter = sub.time_ns.mean() - coef * sub.expanded.mean()
        xs = [sub.expanded.min(), sub.expanded.max()]
        ax.plot(xs, [inter + coef * x for x in xs], color=STYLES[key]["color"],
                linewidth=1.2, linestyle="--")
        print(f"custo por expansão {LABELS[key]:12s}: {coef:8.1f} ns/nó")
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("Nós expandidos (escala log)")
    ax.set_ylabel("Tempo por execução (ns, escala log)")
    ax.set_title("Tempo vs esforço: validação da instrumentação limpa")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(OUT / "fig3_tempo_vs_expansoes.png", dpi=200)
    fig.savefig(OUT / "fig3_tempo_vs_expansoes.pdf")
    plt.close(fig)


def fig_gap(df: pd.DataFrame) -> None:
    d = df[df.source == "benchmark"].drop_duplicates(["family", "n", "instance_idx", "algo",
                                                      "heuristic"])
    fig, ax = plt.subplots(figsize=(7, 4.4))
    for key in ORDER:
        sub = d[d.series == key]
        if sub.empty:
            continue
        g = sub.groupby("n").gap.median().reset_index()
        g.loc[g.gap <= 0, "gap"] = 0.5  # below the log floor = optimal
        ax.plot(g.n, g.gap, label=LABELS[key], linewidth=1.6, markersize=5, **STYLES[key])
    ax.axhline(1, color="gray", linewidth=1, linestyle=":")
    ax.text(4.05, 1.1, "ótimo", fontsize=8, color="gray")
    ax.set_xlabel(XL)
    ax.set_yscale("log")
    ax.set_ylabel("Desvio da solução ótima (minutos, mediana, escala log)")
    ax.set_title("Qualidade da solução: quão longe do ótimo cada método termina")
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(OUT / "fig4_qualidade.png", dpi=200)
    fig.savefig(OUT / "fig4_qualidade.pdf")
    plt.close(fig)


def fig_frontier(df: pd.DataFrame) -> None:
    d = df[df.source == "benchmark"]
    fig, ax = plt.subplots(figsize=(7, 4.4))
    series_line(ax, d, "max_frontier", "median")
    ax.set_ylabel("Tamanho máximo da fronteira (mediana, escala log)")
    ax.set_title("Uso de memória: pico da fronteira de busca")
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(OUT / "fig5_fronteira.png", dpi=200)
    fig.savefig(OUT / "fig5_fronteira.pdf")
    plt.close(fig)


def fig_heuristics(df: pd.DataFrame) -> None:
    d = df[(df.source == "benchmark") & (df.algo == "ASTAR")]
    fig, ax = plt.subplots(figsize=(7, 4.4))
    series_line(ax, d, "expanded", "median")
    ax.set_ylabel("Nós expandidos (mediana, escala log)")
    ax.set_title("Estudo de heurística: força de guia vs esforço (A*)")
    txt = ("h0 = 0 (equivalente a Custo Uniforme)\n"
           "h1 = max restante (admissível)\n"
           "h2 = pares mais lentos (admissível, mais forte)\n"
           "h3 = soma restante (NÃO admissível)")
    ax.text(0.02, 0.98, txt, transform=ax.transAxes, fontsize=7.5,
            verticalalignment="top", bbox=dict(boxstyle="round", facecolor="white", alpha=0.8))
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(OUT / "fig6_heuristicas.png", dpi=200)
    fig.savefig(OUT / "fig6_heuristicas.pdf")
    plt.close(fig)


def fig_heuristics_geometric(df: pd.DataFrame) -> None:
    d = df[(df.source == "benchmark") & (df.algo == "ASTAR") & (df.family == "geometric")]
    fig, ax = plt.subplots(figsize=(7, 4.4))
    series_line(ax, d, "expanded", "median")
    ax.set_ylabel("Nós expandidos (mediana, escala log)")
    ax.set_xlabel(XL)
    ax.set_title("Estudo de heurística em custos geométricos {1,2,4,...} (A*)")
    txt = ("custos crescem em potências de 2:\n"
           "a estrutura favorece heurísticas que\n"
           "enxergam os pares mais lentos")
    ax.text(0.02, 0.98, txt, transform=ax.transAxes, fontsize=7.5,
            verticalalignment="top", bbox=dict(boxstyle="round", facecolor="white", alpha=0.8))
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(OUT / "fig7_heuristicas_geometricas.png", dpi=200)
    fig.savefig(OUT / "fig7_heuristicas_geometricas.pdf")
    plt.close(fig)


def table_classic(df: pd.DataFrame) -> None:
    d = df[df.source == "classic"]
    agg = d.groupby(["algo", "heuristic"], sort=False).agg(
        custo=("solution_cost", "mean"),
        otimo=("optimal_cost", "first"),
        expandidos=("expanded", "mean"),
        gerados=("generated", "mean"),
        fronteira=("max_frontier", "max"),
        tempo_ms=("time_ns", lambda s: s.median() / 1e6),
    ).reset_index()
    agg["otimo?"] = agg.custo.eq(agg.otimo).map({True: "sim", False: "não"})
    agg = agg.rename(columns={"algo": "método", "heuristic": "h"})
    lines = agg.to_markdown(index=False, floatfmt=".2f")
    (OUT / "table_classic.md").write_text(lines + "\n")
    agg.to_csv(OUT / "table_classic.csv", index=False)
    print("\nTabela 1 — instância clássica (médias sobre 30 execuções):")
    print(lines)


def table_scaling(df: pd.DataFrame) -> None:
    d = df[df.source == "benchmark"]
    med = d.groupby(["algo", "heuristic", "n"], sort=False).expanded.median().reset_index()
    piv = med.pivot_table(index=["algo", "heuristic"], columns="n", values="expanded")
    piv.index = [f"{a} ({h})" for a, h in piv.index]
    piv = piv.rename_axis("método").reset_index()
    lines = piv.to_markdown(index=False, floatfmt=".0f")
    (OUT / "table_scaling.md").write_text(lines + "\n")
    print("\nTabela 2 — expansões medianas por n (benchmark):")
    print(lines)


def main() -> int:
    df = load()
    validate(df)
    fig_expansions(df)
    fig_time(df)
    fig_time_vs_expansions(df)
    fig_gap(df)
    fig_frontier(df)
    fig_heuristics(df)
    fig_heuristics_geometric(df)
    table_classic(df)
    table_scaling(df)
    print(f"\nFiguras e tabelas em: {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
