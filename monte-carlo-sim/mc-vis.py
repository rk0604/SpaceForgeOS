
import argparse
import pathlib
import sys

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns
from scipy import stats


# default looks at mc_output6.csv else specify in CLI arg. as such: python mc-vis.py mc_data/mc_output5.csv
def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(add_help=False)        
    p.add_argument("csv", nargs="?",                   
                   help="CSV file to analyse (default: mc_data/mc_output6.csv)")
    p.add_argument("--save", action="store_true", help="Save figures as PNGs")
    p.add_argument("--pairplot", action="store_true", help="Generate seaborn pair-plot")
    return p.parse_args()


# ─────────────────────────────────────────────────── utilities ──
def describe(df: pd.DataFrame, num_cols) -> None:
    print("\n=== Descriptive statistics (numeric columns) ===")
    print(df[num_cols].describe(percentiles=[.05, .25, .5, .75, .95]).round(3))


def correlation_matrix(df: pd.DataFrame, num_cols) -> pd.DataFrame:
    corr = df[num_cols].corr(method="pearson")
    print("\n========= Pearson correlation matrix (rho) =========")
    print(corr.round(3))

    # p-values (Pearson below diag, Spearman above)
    p_pearson  = df[num_cols].corr(method=lambda x, y: stats.pearsonr(x, y)[1]) - np.eye(len(num_cols))
    p_spearman = df[num_cols].corr(method=lambda x, y: stats.spearmanr(x, y)[1]) - np.eye(len(num_cols))
    p_vals = np.tril(p_pearson, k=-1) + np.triu(p_spearman, k=1)
    print("\n(p-values, lower ▷ Pearson  upper ▷ Spearman)")
    print(p_vals.round(3))

    # correlation matrix heat map
    plt.figure(figsize=(7, 6))
    sns.heatmap(corr, annot=True, cmap="coolwarm", fmt=".2f",
                linewidths=0.5, cbar_kws={"label": "rho"})
    plt.title("Parameter correlations")
    plt.tight_layout()
    return corr

# a general scatter plot function
def scatter(df, x, y, hue=None, **kw):
    sns.scatterplot(data=df, x=x, y=y, hue=hue, alpha=0.7, **kw)
    sns.regplot(data=df, x=x, y=y, scatter=False,
                color="k", ci=None, line_kws={"ls": "--"})

# helper function to find the best row, prioritizing a low hit ratio and by using a high deflection angle and a low wake wafer distance as tie breakers
def ranking_row(df) -> pd.Series:
    return df.sort_values(
        ["hit_ratio", "mean_deflection_deg", "wake_wafer_distance"],
        ascending=[True, False, True]
    ).iloc[0]


def main() -> None:
    args = parse_args()

    # default path 
    SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
    DEFAULT_CSV = SCRIPT_DIR / "mc_data" / "mc_output6.csv"
    CSV_PATH = pathlib.Path(args.csv).expanduser() if args.csv else DEFAULT_CSV

    # load the csv file 
    if not CSV_PATH.exists():
        sys.exit(f"Could not find '{CSV_PATH}'")
    df = pd.read_csv(CSV_PATH)
    if df.empty:
        sys.exit("The CSV is empty")
    print(f"\nLoaded {len(df):,} rows from {CSV_PATH}")

    # analysis
    numeric_cols = df.select_dtypes(include=np.number).columns
    print(f"\n========= Describing the numeric data =========\n")
    describe(df, numeric_cols)
    corr = correlation_matrix(df, numeric_cols)

    for col in ["hit_ratio", "wake_wafer_distance"]:
        if col in df.columns and df[col].nunique() == 1:
            print(f"WARNING: {col} is constant ({df[col].iloc[0]}) - correlations are not meaningful.")

    sns.set_theme(style="whitegrid")

    # curvature -> deflection
    plt.figure(figsize=(7, 4))
    scatter(df, "curvature", "mean_deflection_deg", hue="coating_type")
    plt.xlabel("Shield curvature (1/meter)")
    plt.ylabel("AVG. deflection angle")
    plt.title("curvature vs. deflection")
    plt.tight_layout()

    # wake distance -> hit-ratio
    plt.figure(figsize=(7, 4))
    scatter(df, "wake_wafer_distance", "hit_ratio", hue="coating_type")
    plt.xlabel("Wake-to-wafer distance (m)")
    plt.ylabel("Hit ratio")
    plt.title("Wake distance vs. wafer impact probability")
    plt.tight_layout()

    # optional heavy pair-plot
    if args.pairplot:
        pair_vars = ["radius", "curvature", "mass",
                     "wake_wafer_distance", "mean_deflection_deg", "hit_ratio"]
        sns.pairplot(df, vars=pair_vars, hue="coating_type", corner=True,
                     plot_kws={"alpha": 0.5, "s": 25, "edgecolor": "w"})
        plt.suptitle("Pair-wise relationships", y=1.02)

    # design hints
    best_row = ranking_row(df)
    best_coating = df.groupby("coating_type")["mean_deflection_deg"].mean().idxmax()

    print("\n=== Guiding conclusions ===")
    print(f"• Coating with highest *mean* deflection: {best_coating}")
    print(f"• ρ(curvature, deflection) : {corr.loc['curvature', 'mean_deflection_deg']:+.2f}")
    print(f"• ρ(radius, deflection)    : {corr.loc['radius', 'mean_deflection_deg']:+.2f}")
    print(f"• ρ(wake_dist, hit_ratio)  : {corr.loc['wake_wafer_distance', 'hit_ratio']:+.2f}")

    print("\n🏆 Top performer (min hit_ratio → max deflection → min wake_dist)")
    print(f"    radius              = {best_row['radius']:.3f} m")
    print(f"    curvature           = {best_row['curvature']:.2f} 1/m")
    print(f"    wake-wafer distance = {best_row['wake_wafer_distance']:.3f} m")
    print(f"    coating             = {best_row['coating_type']}")
    print(f"    deflection          = {best_row['mean_deflection_deg']:.2f} °")
    print(f"    hit_ratio           = {best_row['hit_ratio']:.4f}")

    # output: save or show
    if args.save:
        out_dir = CSV_PATH.with_suffix("")  # “mc_output6” → directory
        out_dir.mkdir(exist_ok=True)
        for i, fignum in enumerate(plt.get_fignums(), 1):
            plt.figure(fignum)
            fname = out_dir / f"fig{i:02}.png"
            plt.savefig(fname, dpi=300)
            print(f"  ↳ saved {fname}")
    else:
        plt.show()


if __name__ == "__main__":
    main()
