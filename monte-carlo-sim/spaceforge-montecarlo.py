import pandas as pd
import numpy as np
from math3d import *
from physics import trace_batch
from geometry import Shield, WaferPlane, Scene, WakeCone
import random
import seaborn as sns
sns.set_theme(style="darkgrid")


def generate_random_float_tuple(length, min_val, max_val):
    return tuple(random.uniform(min_val, max_val) for _ in range(length))


def main():
    records = []
    skipped  = 0              # track runs where wafer is outside the cone

    for i in range(10000): # 1,000 scenes - 10,000 scenes
        shieldSample = Shield(
            radius=np.random.uniform(0.3, 1.0),
            curvature=np.random.uniform(1.5, 3.0),
            # mc_output1.csv – curvature = np.random.uniform(0.0, 1.0)
            thickness=np.random.uniform(1.5, 3.0),           # was 0.01 cm for mc_output1-3
            coating_type=random.choice(["specular", "diffuse"])
        )

        # ---------------------- random wafer ----------------------
        xy_offset = generate_random_float_tuple(2, -0.05, 0.05)      # small misalignment in x/y
        waferSample = WaferPlane(
            radius = 0.15,                                             # 150 mm – standard production wafer
            #  z_offset = np.random.uniform(0.005, 0.05),            # 0.5 cm–5 cm IN FRONT of shield
            z_offset = -np.random.uniform(0.1, 0.2),                   # 10–20 cm BEHIND the shield (–Z)
            xy_offset = xy_offset
        )

        # ----------------------  wake cone tied to this geometry ----------------------
        #   half-angle chosen so cone walls just graze wafer edge (+1 degree safety margin)
        half_angle_deg = np.degrees(np.arctan(waferSample.radius / abs(waferSample.z_offset))) + 1.0
        wakeConeSample = WakeCone(
            half_angle_deg=half_angle_deg,
            length=shieldSample.radius * 10.0                        # empirical 10 * Shield_radius rule
        )

        # ensure wafer actually sits inside the cone cross-section at its Z
        distance_to_wafer    = abs(waferSample.z_offset)   # axial distance from apex
        wake_radius_at_wafer = distance_to_wafer * np.tan(np.radians(wakeConeSample.half_angle_deg))

        # shield too small and hence the wake its producing is too small as well so skip the run
        if waferSample.radius > wake_radius_at_wafer:
            skipped += 1  
            continue
        
        wake_wafer_distance = wake_radius_at_wafer - waferSample.radius

        # ---------------------- build scene & run Monte-Carlo ----------------------
        scene = Scene(shield=shieldSample, wafer=waferSample, wake=wakeConeSample)
        mean_deflect, hit_ratio, wake_ratio = trace_batch(scene, batch_size=10_000) # batch size increased to 10,000 particles per scene 

        records.append({
            "radius":            shieldSample.radius,
            "curvature":         shieldSample.curvature,
            "thickness":         shieldSample.thickness,
            "coating_type":      shieldSample.coating_type,
            "z_offset":          waferSample.z_offset,
            "xy_offset_x":       xy_offset[0],
            "xy_offset_y":       xy_offset[1],
            "mass":              shieldSample.mass(),
            "mean_deflection_deg": mean_deflect,
            "hit_ratio":           hit_ratio,
            "wake_intrusion_ratio": wake_ratio,
            "wake_wafer_distance": wake_wafer_distance
        })

    # ---------------------- save + quick viz ----------------------
    df = pd.DataFrame.from_records(records)
    df.to_csv("mc_data/mc_output2.csv", index=False)
    print(f"Saved {len(df)} valid runs to mc_output2.csv   |   {skipped} skipped (wafer outside wake)")

    sns.relplot(data=df, x="radius", y="hit_ratio")
    # sns.relplot(data=df, x="radius", y="wake_intrusion_ratio")   # uncomment to inspect wake metric


if __name__ == "__main__":
    main()
