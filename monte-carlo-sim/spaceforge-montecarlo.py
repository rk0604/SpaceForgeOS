import pandas as pd
import numpy as np
from math3d import *
from physics import trace_batch
from geometry import (
    Shield, WaferPlane, Scene,
    WakeCone,        
    PyramidWake      
)
import random
import seaborn as sns
sns.set_theme(style="darkgrid")
import matplotlib.pyplot as plt

# save every scene's wafer hits to a parquet file 
import pyarrow as pa
import pyarrow.parquet as pq
from pathlib import Path

FILE_NO = 5

def generate_random_float_tuple(length, min_val, max_val):
    return tuple(random.uniform(min_val, max_val) for _ in range(length))


def main():
    records = []
    skipped = 0              # track runs where wafer is outside the wake
    profiles = ["cap", "flat", "pyramid", "cupola"]
    
    # accumulate all hits (same shape as wafer grid)
    total_grid = np.zeros((50, 50), dtype=float)

    # optional: per-profile accumulation
    grids_by_profile = {p: np.zeros((50,50), float) for p in profiles}
    
    def random_shield(profile: str) -> Shield:
        """
        Return a Shield instance whose random parameters are sensible for the
        chosen profile.  'radius' means:
            • cap / flat  : true circular radius   (m)
            • pyramid     : half-base              (m)
            • cupola (J5) : *edge length*          (m)  - see geometry.py
        """
        thickness_mm = np.random.uniform(1.5, 3.0)          # wall 1.5–3 mm

        if profile == "cupola":                              # Johnson J5 pentagonal cupola
            edge_len  = np.random.uniform(1.0, 6.0)          # 2–3 m edges for mc 1 and then 1-2m for mc 2, then 5-6 mc3
            radius    = edge_len
            curvature = 1.0                                  # placeholder
        elif profile == "pyramid":
            radius    = np.random.uniform(1.0, 4.0)          # half-base, for mc2 1-2 and mc1 is 3-4, then 2-4 for mc3
            curvature = np.random.uniform(0.5, 1.5)          # h/r aspect
        elif profile == "flat":
            radius    = np.random.uniform(1.0, 6.0)          # mc1: 3-4, mc2: 1-2, mc3: 5-6
            curvature = 0.0
        else:                             # cap or flat disk
            radius    = np.random.uniform(1.0, 6.0)          # mc1: 3–4, mc2: 1-2, mc3: 5-6
            curvature = np.random.uniform(1.5, 3.0)          # sphere-cap k

        return Shield(
            primary_dim  = radius,
            shape_param  = curvature,
            thickness    = thickness_mm / 1_000,             # conversion to meters
            coating_type = random.choice(["specular", "diffuse"]),
            profile      = profile,
        )
        
    # ---- Parquet output (per-scene grids) ----
    PARQUET_OUT = Path(f"mc_data/grids_mc{FILE_NO}.parquet")
    PARQUET_OUT.parent.mkdir(parents=True, exist_ok=True)

    # We will stream-write in chunks so RAM stays low.
    writer = None         # pq.ParquetWriter, created on first flush
    row_buffer = []       # list[dict]
    CHUNK_SIZE = 500      # write every 500 scenes; tweak as you like
    grid_cols = None      # will hold g000 to g2499, determined on first grid

    for i in range(10_000):   # 10 000 scenes
        profile_choice = random.choice(profiles)

        # ----------------------  shield ----------------------
        thickness_mm = np.random.uniform(1.5, 3.0)
        shieldSample   = random_shield(profile_choice)

        # ---------------------- wafer plane ------------------
        xy_offset = generate_random_float_tuple(2, -0.05, 0.05)
        waferSample = WaferPlane(
            radius    = 0.30,                                           # 300 mm wafer for mc1-4 and 60mm wafer for mc5
            z_offset  = -np.random.uniform(1.0, 1.5),                      # 0.5-1.0 for mc1, and 1.0-1.5 for mc2
            xy_offset = xy_offset,
        )

        # ---------------------- wake geometry ----------------
        if profile_choice == "pyramid":
            # square-pyramid wake, same slope as shield faces
            shield_h   = max(shieldSample.shape_param, 0.3) * shieldSample.primary_dim
            wake_len   = shieldSample.primary_dim * 10.0           # match the cone heuristic
            wake_obj   = PyramidWake(half_base=shieldSample.primary_dim,
                                     length    =wake_len)
            # wafer-inside test
            wafer_pt   = np.array([[*xy_offset, waferSample.z_offset]])
            if not wake_obj.contains(wafer_pt, shield_h)[0]:
                skipped += 1
                continue

        else:
            # axis-symmetric cone (cap & flat)
            half_angle = (
                np.degrees(np.arctan(shieldSample.primary_dim / abs(waferSample.z_offset)))
            )
            wake_obj = WakeCone(
                half_angle_deg = half_angle,
                length         = shieldSample.primary_dim * 10.0
            )
            # wafer-inside test
            dist  = abs(waferSample.z_offset)
            r_max = dist * np.tan(np.radians(wake_obj.half_angle_deg))
            if waferSample.radius > r_max:
                skipped += 1
                continue

        # ---------------------- run Monte-Carlo --------------
        scene = Scene(shield=shieldSample, wafer=waferSample, wake=wake_obj)
        mean_defl, hit_ratio, wake_ratio, wafer_flux_m2s, grid = trace_batch(scene, batch_size=10_000)
        
        # ---- build one Parquet row: metadata + flattened grid ----
        if grid_cols is None:
            H, W = grid.shape
            grid_cols = [f"g{k:04d}" for k in range(H * W)]  # g0000..g{H*W-1}
        
        total_grid += grid
        grids_by_profile[shieldSample.profile] += grid

        # ---------------------- collect records ----------------
        rec = {
            "profile":               shieldSample.profile,
            "primary_dim":           shieldSample.primary_dim,
            "shape_param":           shieldSample.shape_param,    # cap: 1/R    # flat: theta or ignored    # pyramid: aspect ratio (h/half-base)   cupola: placeholder, has no effect
            "thickness":             shieldSample.thickness,
            "coating_type":          shieldSample.coating_type,
            "z_offset":              waferSample.z_offset,
            "xy_offset_x":           xy_offset[0],
            "xy_offset_y":           xy_offset[1],
            "mass":                  shieldSample.mass(),
            "mean_deflection_deg":   mean_defl,
            "hit_ratio":             hit_ratio,
            "wafer_flux_m2s":        wafer_flux_m2s,                # The average number of real LEO particles that strike one square meter of wafer per second
            "wake_intrusion_ratio":  wake_ratio,
            "wake_type":             wake_obj.__class__.__name__,   # Cone vs PyramidWake
        }
        records.append(rec)

        # ---- build and buffer Parquet row (same metadata + grid cells) ----
        row = dict(rec)  # shallow copy so we can add grid values
        gf = grid.astype("uint32").ravel(order="C")
        row.update({col: int(val) for col, val in zip(grid_cols, gf)})
        row_buffer.append(row)

        # ---- flush to Parquet every CHUNK_SIZE rows ----
        if len(row_buffer) >= CHUNK_SIZE:
            table = pa.Table.from_pylist(row_buffer)
            if writer is None:
                writer = pq.ParquetWriter(str(PARQUET_OUT), table.schema, compression="zstd")
            writer.write_table(table)
            row_buffer.clear()

    # ---------------------- final flush to Parquet -----------------
    if row_buffer:
        table = pa.Table.from_pylist(row_buffer)
        if writer is None:
            writer = pq.ParquetWriter(str(PARQUET_OUT), table.schema, compression="zstd")
        writer.write_table(table)
        row_buffer.clear()
    if writer is not None:
        writer.close()
    print(f"Wrote per-scene grids to {PARQUET_OUT}")

    # ---------------------- save + quick viz -----------------
    df = pd.DataFrame.from_records(records)
    CSV_OUT = f"mc_output{FILE_NO}"
    df.to_csv(f"mc_data/{CSV_OUT}.csv", index=False)
    print(f"Saved {len(df):,} valid runs -> {CSV_OUT}.csv   |   {skipped:,} skipped (wafer outside wake)")
    
    # plot ONCE at the end
    '''
    R = 0.15
    plt.figure(figsize=(5,4))
    plt.imshow(total_grid, origin="lower", extent=[-R, R, -R, R])
    plt.xlabel("x (m)"); plt.ylabel("y (m)")
    plt.title("Wafer hit density (50 x 50) — all scenes")
    plt.colorbar(label="hit counts")
    plt.tight_layout(); plt.show()    
    '''


if __name__ == "__main__":
    main()
