import numpy as np

from math3d import (
    rand_unit_vector_cosine_weighted,
    plane_intersection,          # wafer plane hit
    sphere_intersection,         # spherical shield hit
    disk_hit,                    # point-in-wafer test
    rays_hit_infinite_cone,      # NEW – wake-cone intersection test
    sphere_intersection_batch,
    disk_hit_batch,
    plane_intersection_batch
)
from geometry import Scene

# Physical constants for LEO molecular flux ---------------------------------------------------------------------
'''
    Real Low Earth Orbit (LEO) gas is a rarefied thermal flow riding on orbital drift. A constant speed of 8km/s ignored the temperature effects 
'''
K_B         = 1.380649e-23      # Boltzmann constant  [J/K]
MASS_O2     = 2.656e-26         # kg  (molecular oxygen, dominant at 300 km)
T_EXO       = 1000.0            # K   (typical exospheric temperature)
ORBITAL_VEL = 7_700.0           # m/s relative flow speed at 300 km

# Pre‑compute thermal speed scale (1‑D Maxwell omega)
V_TH = np.sqrt(K_B * T_EXO / MASS_O2)

# Particle source ---------------------------------------------------------------------

def sample_incident(scene: Scene, size: int, rng=None):
    """
    Generate particles above the shield.
    • Origins   : random disc at Z = +1 m, radius = 1.5 * shield.radius
    • Velocities: orbital drift (-Z)  + Maxwell-Boltzmann thermal spread drift direction is cosine-weighted about -Z.
    """
    rng = np.random if rng is None else rng

    # origin of the particles
    r_max = scene.shield.radius * 1.5
    theta = rng.uniform(0.0, 2.0 * np.pi, size)
    r     = r_max * np.sqrt(rng.uniform(0.0, 1.0, size))

    x = r * np.cos(theta)
    y = r * np.sin(theta)
    z = np.full_like(x, 1.0)                 # 1 m above shield
    pos = np.stack([x, y, z], axis=1)

    # ---- velocity (orbital + thermal)  ----
    v_dir   = -rand_unit_vector_cosine_weighted(size)   # mean flow toward −Z
    v_drift = v_dir * ORBITAL_VEL                       # bulk orbital component

    thermal = rng.normal(0.0, V_TH, (size, 3))          # 3‑D Maxwellian spread
    vel     = v_drift + thermal

    return pos, vel

# ---------------------------------------------------------------------
# Reflection helpers (unchanged)
# ---------------------------------------------------------------------

def reflect_specular(v_in: np.ndarray, normal: np.ndarray) -> np.ndarray:
    """Perfect mirror reflection."""
    return v_in - 2.0 * np.dot(v_in, normal) * normal


def reflect_diffuse(normal: np.ndarray, rng=None) -> np.ndarray:
    """Lambertian (cosine-weighted) reflection."""
    rng   = np.random if rng is None else rng
    local = rand_unit_vector_cosine_weighted(1)[0]   # +Z hemisphere

    z_axis = np.array([0.0, 0.0, 1.0])
    axis   = np.cross(z_axis, normal)
    axis_n = np.linalg.norm(axis)
    if axis_n < 1e-6:                 # already aligned
        return local if normal[2] > 0 else -local

    axis  /= axis_n
    angle  = np.arccos(np.clip(np.dot(z_axis, normal), -1.0, 1.0))
    return rotate_vector(local, axis, angle)


def rotate_vector(v: np.ndarray, k: np.ndarray, angle: float) -> np.ndarray:
    """Rodrigues' rotation formula."""
    return (
        v * np.cos(angle)
        + np.cross(k, v) * np.sin(angle)
        + k * np.dot(k, v) * (1.0 - np.cos(angle))
    )

# Main batch tracer ---------------------------------------------------------------------
def trace_batch(scene: Scene, batch_size: int, rng=None):
    """
    mean_deflection_deg : float
        Average angle between -v_in and v_out (deg).  0° = perfect back-scatter
        180° = no deflection
    hit_ratio : float
        Fraction of particles striking the wafer
    wake_intrusion_ratio : float
        Fraction of particles whose ray intersects the wake cone
    """
    rng = np.random if rng is None else rng

    # ----------------------------------------------------------- initial sample
    pos, vel = sample_incident(scene, batch_size, rng)

    # -------------- Guard against impossible cap --------------
    R_sphere = 1.0 / scene.shield.curvature
    if scene.shield.radius > R_sphere:          # cap must not exceed hemisphere
        R_sphere = scene.shield.radius * 0.99   # local surrogate only

    sphere_ctr = np.array([0.0, 0.0, R_sphere])        # convex face upstream
    wafer_ctr  = np.array([*scene.wafer.xy_offset, scene.wafer.z_offset])

    # ---------------------------------------------------- normalise velocities
    # v_in_unit  = vel[i] / np.linalg.norm(vel[i])
    v_in_unit = vel / np.linalg.norm(vel, axis=1, keepdims=True)   # (N,3)

    # --------------------------------------------------- shield intersections
    # hit_pt = sphere_intersection(pos[i], v_in_unit, -R_sphere)
    hit_pts = sphere_intersection_batch(pos, v_in_unit, -R_sphere)  # (N, 3)

    # ----------------------- physical rim clip (outside shield radius ⇒ NaN)
    r2        = hit_pts[:, 0] ** 2 + hit_pts[:, 1] ** 2
    rim_mask  = r2 <= scene.shield.radius ** 2
    hit_pts[~rim_mask] = np.nan

    miss_mask = np.isnan(hit_pts).any(axis=1)         # True ⇒ no shield hit
    hit_mask  = ~miss_mask

    # -------------------------------------------------- build outgoing vectors
    ray_pos = np.where(miss_mask[:, None], pos, hit_pts)  # (N,3)

    ray_dir = np.empty_like(vel)

    # -- case A: MISS (no reflection) -----------------------------------------
    ray_dir[miss_mask] = v_in_unit[miss_mask]          # straight through
    defl_deg = np.full(batch_size, 180.0)              # default 180°

    # -- case B: HIT (specular or diffuse) ------------------------------------
    if hit_mask.any():
        normals = (hit_pts[hit_mask] - sphere_ctr) / R_sphere   # (Nh,3)

        if scene.shield.coating_type == "specular":
            # v_out = v_in - 2(n·v_in)n   (vectorised)
            dot_nv      = np.einsum("ij,ij->i", v_in_unit[hit_mask], normals)
            v_out       = v_in_unit[hit_mask] - 2.0 * dot_nv[:, None] * normals
        else:
            # Lambertian cosine-weighted reflection, vectorised
            local_dirs  = rand_unit_vector_cosine_weighted(normals.shape[0])
            z_axis      = np.array([0.0, 0.0, 1.0])
            axes        = np.cross(np.broadcast_to(z_axis, normals.shape), normals)
            axes_norm   = np.linalg.norm(axes, axis=1, keepdims=True)
            # Rodrigues rotation of every local_dir → align with each normal
            cosA        = normals[:, 2]                        # dot(+Z, n)
            sinA        = np.sqrt(np.clip(1.0 - cosA**2, 0.0, 1.0))
            k_unit      = np.divide(axes, axes_norm,
                                    out=np.zeros_like(axes),
                                    where=axes_norm > 1e-8)
            term1       = local_dirs * cosA[:, None]
            term2       = np.cross(k_unit, local_dirs) * sinA[:, None]
            term3_scale = np.einsum("ij,ij->i", k_unit, local_dirs)
            term3       = k_unit * term3_scale[:, None] * (1.0 - cosA)[:, None]
            v_out       = term1 + term2 + term3
            # handle n ≈ ±Z (axes_norm==0): simply flip local_dir if needed
            aligned     = axes_norm[:, 0] < 1e-8
            v_out[aligned & (normals[:, 2] < 0)] *= -1.0

        ray_dir[hit_mask] = v_out

        # ------------------ deflection angle for hits ------------------------
        cos_th = np.einsum("ij,ij->i", -v_in_unit[hit_mask], v_out)
        defl_deg[hit_mask] = np.degrees(np.arccos(np.clip(cos_th, -1.0, 1.0)))

    # ------------------------------------------------ global deflection stats
    mean_deflection_deg = defl_deg.mean()

    # ------------------------------------------------------ wake-cone test ---
    apex        = np.zeros(3)
    axis        = scene.wake.axis
    cos2_alpha  = scene.wake.cos2
    wake_mask   = rays_hit_infinite_cone(ray_pos, ray_dir, apex, axis, cos2_alpha)

    # --------------------------------------------------- wafer intersection --
    wafer_pts   = plane_intersection_batch(ray_pos, ray_dir, scene.wafer.z_offset)
    wafer_hit_mask = ~np.isnan(wafer_pts).any(axis=1) & disk_hit_batch(
        wafer_pts, wafer_ctr, scene.wafer.radius
    )

    # A wafer hit is, by design, also a wake hit
    wake_hits   = np.count_nonzero(wake_mask | wafer_hit_mask)
    wafer_hits  = np.count_nonzero(wafer_hit_mask)

    hit_ratio            = wafer_hits  / batch_size
    wake_intrusion_ratio = wake_hits   / batch_size

    return mean_deflection_deg, hit_ratio, wake_intrusion_ratio
