## 🗂️ Simulation Log  ▸  **mc_output1.csv**

| Item | Value / Range | Notes |
|------|---------------|-------|
| **Shield geometry** | **Spherical cap** (new) | Replaced previous flat-disk approximation. Radius of curvature `R = 1/curvature`. |
| **Swept radius** | 0.30 – 1.00 m | Uniform random. |
| **Curvature** | 1.50 – 3.00 (1/m) | Uniform random → sphere radii 0.33 – 0.67 m. |
| **Thickness** | 0.01 m | Fixed (constant across runs). |
| **Coating type** | {specular, diffuse} | 50 / 50 random draw. |
| **Wafer plane** | 0.5 – 5 cm **behind** shield ( *z* = –0.005 … –0.05 m ) | `z_offset` uniform.<br>`xy_offset` ±5 cm mis-alignment. |
| **Particle source** | Disk radius = 1.1 × shield radius at *z* = +1 m.<br>Directions cosine-weighted toward –Z.<br>Speed = 8000 m s⁻¹. | Same as earlier revision. |
| **Batch size** | 1 000 particles / scene | 1 000 shield/wafer scenes ⇒ 1 000 000 rays. |
| **Primary metric** | **`hit_ratio` (minimised)** | *mc_vis* now ranks scenes by lowest wafer-hit fraction. |
| **Secondary metric** | `mean_deflection_deg` | Still recorded for diagnostics. |

### Code changes introduced in this run
| File | Change |
|------|--------|
| **`math3d.py`** | + `sphere_intersection()` helper for ray–sphere hits. |
| **`physics.py`** | • Replaced `plane_intersection(... plane_z=0)` with `sphere_intersection()`.<br>• Correct surface normal: `(hit − sphere_center) / R`.<br>• Optional comments clarified coordinate convention. |
| **`mc-vis.py`** | • CSV path now points to **`mc_output1.csv`**.<br>• Best design selected with `df['hit_ratio'].idxmin()` (not by deflection). |
| **`spaceforge-montecarlo.py`** | CSV output renamed to **`mc_output1.csv`** for this configuration. |

### Key output excerpt (`mc_vis`)
=== Correlation matrix (rounded to 3 d.p.) ===
                     radius  curvature  thickness  z_offset  xy_offset_x  xy_offset_y   mass  mean_deflection_deg  hit_ratio
radius                1.000     -0.039        NaN     0.003       -0.022        0.023  0.991               -0.356        NaN
curvature            -0.039      1.000        NaN    -0.031        0.022       -0.019 -0.035               -0.216        NaN
thickness               NaN        NaN        NaN       NaN          NaN          NaN    NaN                  NaN        NaN
z_offset              0.003     -0.031        NaN     1.000        0.057        0.027  0.005               -0.013        NaN
xy_offset_x          -0.022      0.022        NaN     0.057        1.000        0.049 -0.026                0.012        NaN
xy_offset_y           0.023     -0.019        NaN     0.027        0.049        1.000  0.020                0.019        NaN
mass                  0.991     -0.035        NaN     0.005       -0.026        0.020  1.000               -0.360        NaN
mean_deflection_deg  -0.356     -0.216        NaN    -0.013        0.012        0.019 -0.360                1.000        NaN
hit_ratio               NaN        NaN        NaN       NaN          NaN          NaN    NaN                  NaN        NaN

⚠️  WARNING: hit_ratio is constant (0.0 for all rows) — no correlation values can be computed.

=== Guiding conclusions ===
• Coating with highest mean deflection ..........: specular
• Curvature trend rho(curvature, deflection) .......: -0.22
• Radius trend    rho(radius, deflection) ..........: -0.36

🏆 Top performer (current criterion):
    radius     = 0.315 m
    curvature  = 2.01 1/m
    coating    = specular
    deflection = 65.44 °
    hit_ratio  = 0.0000
