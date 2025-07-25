#!/usr/bin/env python3
# ---------------------------------------------------------------------
# SpaceForgeOS – Monte-Carlo shield study visualiser
# ---------------------------------------------------------------------
import pathlib, sys
import pandas as pd, numpy as np
import seaborn as sns, matplotlib.pyplot as plt

# ---------------------------------------------------------------------#
# 0.  Load CSV                                                         #
# ---------------------------------------------------------------------#
CSV = pathlib.Path(__file__).parent / "mc_data" / "mc_output1.csv"
if not CSV.exists():
    sys.exit(f"❌  Could not find {CSV}")

df = pd.read_csv(CSV)

# ---------------------------------------------------------------------#
# 1.  Correlation matrix                                               #
# ---------------------------------------------------------------------#
num_cols = df.select_dtypes(include=np.number).columns
corr = df[num_cols].corr()

print("\n=== Correlation matrix (rounded to 3 d.p.) ===")
print(corr.round(3))

plt.figure(figsize=(6, 5))
sns.heatmap(
    corr,
    annot=True,
    cmap="coolwarm",
    fmt=".2f",
    linewidths=.5,
    cbar_kws=dict(label="ρ"),
)
plt.title("Parameter correlations")
plt.tight_layout()

# Warn if hit_ratio is constant
if "hit_ratio" in df and df["hit_ratio"].nunique() == 1:
    print(
        "\n⚠️  WARNING: hit_ratio is constant "
        f"({df['hit_ratio'].iloc[0]} for all rows) — "
        "no correlation values can be computed."
    )

# ---------------------------------------------------------------------#
# 2.  Pair-plot                                                        #
# ---------------------------------------------------------------------#
sns.set_theme(style="whitegrid")
pair_vars = ["radius", "curvature", "mass", "mean_deflection_deg", "hit_ratio"]
sns.pairplot(
    df,
    vars=pair_vars,
    hue="coating_type",
    corner=True,
    plot_kws=dict(alpha=0.6, s=40, edgecolor="w"),
)
plt.suptitle("Pairwise relationships", y=1.02)

# ---------------------------------------------------------------------#
# 3.  Scatter: curvature → deflection                                  #
# ---------------------------------------------------------------------#
plt.figure(figsize=(7, 4))
sns.scatterplot(
    data=df,
    x="curvature",
    y="mean_deflection_deg",
    hue="coating_type",
    alpha=0.7,
)
sns.regplot(
    data=df,
    x="curvature",
    y="mean_deflection_deg",
    scatter=False,
    color="k",
    ci=None,
    line_kws=dict(ls="--"),
)
plt.xlabel("Shield curvature (1/m)")
plt.ylabel("Mean deflection (°)")
plt.title("Curvature vs. deflection")
plt.tight_layout()

# ---------------------------------------------------------------------#
# 4.  Scatter: radius → deflection (colour by curvature)               #
# ---------------------------------------------------------------------#
plt.figure(figsize=(7, 4))
sns.scatterplot(
    data=df,
    x="radius",
    y="mean_deflection_deg",
    hue="curvature",
    palette="viridis",
    alpha=0.7,
)
plt.xlabel("Shield radius (m)")
plt.ylabel("Mean deflection (°)")
plt.title("Radius vs. deflection")
plt.tight_layout()

# ---------------------------------------------------------------------#
# 5.  Simple design recommendations                                    #
# ---------------------------------------------------------------------#
best_defl_coating = (
    df.groupby("coating_type")["mean_deflection_deg"].mean().idxmax()
)

if df["hit_ratio"].nunique() > 1:
    best_row = df.loc[df["hit_ratio"].idxmin()]
else:
    best_row = df.loc[df["mean_deflection_deg"].idxmax()]

print("\n=== Guiding conclusions ===")
print(f"• Coating with highest mean deflection ..........: {best_defl_coating}")
print(
    "• Curvature trend rho(curvature, deflection) .......: "
    f"{corr.loc['curvature','mean_deflection_deg']:+.2f}"
)
print(
    "• Radius trend    rho(radius, deflection) ..........: "
    f"{corr.loc['radius','mean_deflection_deg']:+.2f}"
)
print(
    f"\n🏆 Top performer (current criterion):\n"
    f"    radius     = {best_row['radius']:.3f} m\n"
    f"    curvature  = {best_row['curvature']:.2f} 1/m\n"
    f"    coating    = {best_row['coating_type']}\n"
    f"    deflection = {best_row['mean_deflection_deg']:.2f} °\n"
    f"    hit_ratio  = {best_row['hit_ratio']:.4f}"
)

plt.show()
