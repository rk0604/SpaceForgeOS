import numpy as np

from math3d import (
    rand_unit_vector_cosine_weighted,
    plane_intersection,          # wafer plane hit
    sphere_intersection,         # spherical shield hit
    disk_hit,                    # point-in-wafer test
    rays_hit_infinite_cone,      # NEW – wake-cone intersection test
)
from geometry import Scene

SPEED_MEAN = 8_000.0  # m/s  (approx orbital velocity)

# ---------------------------------------------------------------------
# Particle source
# ---------------------------------------------------------------------
def sample_incident(scene: Scene, size: int, rng=None):
    """
    Generate `size` particles above the shield.

    • Origins  : random disc at Z = +1 m, radius = 1.5 x shield.radius
    • Velocities: cosine-weighted directions aimed toward -Z
    """
    rng = np.random if rng is None else rng

    r_max = scene.shield.radius * 1.5
    theta = rng.uniform(0.0, 2.0 * np.pi, size)
    r     = r_max * np.sqrt(rng.uniform(0.0, 1.0, size))

    x = r * np.cos(theta)
    y = r * np.sin(theta)
    z = np.full_like(x, 1.0)                 # 1 m above shield

    pos = np.stack([x, y, z], axis=1)

    v_dir = -rand_unit_vector_cosine_weighted(size)  # point toward –Z
    vel   = v_dir * SPEED_MEAN

    return pos, vel


# ---------------------------------------------------------------------
# Reflection helpers
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


# ---------------------------------------------------------------------
# Main batch tracer
# ---------------------------------------------------------------------
def trace_batch(scene: Scene, batch_size: int, rng=None):
    """
    Returns
    --------
    mean_deflection_deg : float
        Average angle between -v_in and v_out (deg).  0° = perfect back-scatter,
        180° = no deflection.
    hit_ratio : float
        Fraction of particles that strike the wafer.
    wake_intrusion_ratio : float
        Fraction of particles whose ray intersects the wake cone.
    """
    rng = np.random if rng is None else rng

    # initial sample
    pos, vel = sample_incident(scene, batch_size, rng)

    wafer_hits   = 0
    wake_hits    = 0
    total_theta  = 0.0

    # Convenience values (wake geometry)
    apex       = np.zeros(3)            # shield centre alligned with wake cone tip
    axis       = scene.wake.axis        # global –Z  [0.0, 0.0, -1.0] - pointing towards the wafer (downstream)
    cos2_alpha = scene.wake.cos2

    # spherical cap parameters
    R_sphere   = 1.0 / scene.shield.curvature
    sphere_ctr = np.array([0.0, 0.0, R_sphere])

    wafer_ctr  = np.array(
        [scene.wafer.xy_offset[0],
         scene.wafer.xy_offset[1],
         scene.wafer.z_offset]
    )

    for i in range(batch_size):
        v_in       = vel[i]
        v_in_unit  = v_in / np.linalg.norm(v_in)

        # ---------- Shield intersection ----------
        hit_pt = sphere_intersection(pos[i], v_in_unit, -R_sphere)
        
        if not np.any(np.isnan(hit_pt)):
            r2 = hit_pt[0]**2 + hit_pt[1]**2
            if r2 > scene.shield.radius**2:          # outside physical rim
                hit_pt = np.full(3, np.nan)          # treat as MISS

        if np.any(np.isnan(hit_pt)):          # MISS: no reflection
            ray_pos = pos[i]
            ray_dir = v_in_unit

            # deflection = 180 (no change)
            total_theta += 180.0
        else:                                 # HIT: reflect
            normal = (hit_pt - sphere_ctr) / R_sphere

            if scene.shield.coating_type == "specular":
                v_out = reflect_specular(v_in_unit, normal)
            else:
                v_out = reflect_diffuse(normal, rng)

            ray_pos = hit_pt
            ray_dir = v_out

            # deflection angle between –v_in and v_out
            cos_th = np.clip(np.dot(-v_in_unit, v_out), -1.0, 1.0)
            total_theta += np.degrees(np.arccos(cos_th))

        # ---------- Wake-cone check ----------
        if rays_hit_infinite_cone(ray_pos[None, :], ray_dir[None, :],
                                  apex, axis, cos2_alpha)[0]:
            wake_hits += 1

        # ---------- Wafer check ----------
        wafer_pt = plane_intersection(ray_pos, ray_dir, scene.wafer.z_offset)
        if np.any(np.isnan(wafer_pt)):
            continue

        if disk_hit(wafer_pt, wafer_ctr, scene.wafer.radius):
            wafer_hits += 1
            wake_hits  += 1          # wafer lies inside the cone by design

    mean_deflection_deg   = total_theta / batch_size
    hit_ratio             = wafer_hits / batch_size
    wake_intrusion_ratio  = wake_hits / batch_size

    return mean_deflection_deg, hit_ratio, wake_intrusion_ratio
