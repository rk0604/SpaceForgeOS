import pandas as pd
import numpy as np
from math3d import *
from physics import trace_batch
from geometry import Shield, WaferPlane, Scene
import random
import seaborn as sns
sns.set_theme(style="darkgrid")


def generate_random_float_tuple(length, min_val, max_val):
    return tuple(random.uniform(min_val, max_val) for _ in range(length))

def main():
    records = []

    for i in range(1000):
        shieldSample = Shield(
            radius=np.random.uniform(0.3, 1.0),
            curvature=np.random.uniform(1.5, 3.0),
            # mc_ouput1.csv - curvature=np.random.uniform(0.0, 1.0),
            thickness=0.01,
            coating_type=random.choice(["specular", "diffuse"])
        )

        xy_offset = generate_random_float_tuple(2, -0.05, 0.05)  # small misalignment in x/y
        waferSample = WaferPlane(
            radius=0.15,
            #  z_offset=np.random.uniform(0.005, 0.05),  # .5 cm to 5 cm IN FRONT of the shield 
            # z_offset = -random.uniform(0.05, 0.30), # 5cm to 30cm IN FRONT OF THE shield
            z_offset = -np.random.uniform(0.005, 0.05),   # 0.5–5 cm BEHIND the shield
            xy_offset=xy_offset
        )

        scene = Scene(shield=shieldSample, wafer=waferSample)
        mean_deflect, hit_ratio = trace_batch(scene, batch_size=1_000)

        records.append({
            "radius": shieldSample.radius,
            "curvature": shieldSample.curvature,
            "thickness": shieldSample.thickness,
            "coating_type": shieldSample.coating_type,
            "z_offset": waferSample.z_offset,
            "xy_offset_x": xy_offset[0],
            "xy_offset_y": xy_offset[1],
            "mass": shieldSample.mass(),
            "mean_deflection_deg": mean_deflect,
            "hit_ratio": hit_ratio
        })

    df = pd.DataFrame.from_records(records)
    df.to_csv("mc_data/mc_output1.csv", index=False)
    print("Saved results to mc_output1.csv")
    
    sns.relplot(data=df, x="radius", y="hit_ratio")
    
if __name__ == "__main__":
    main()

