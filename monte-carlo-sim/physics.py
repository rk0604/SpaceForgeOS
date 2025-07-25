import numpy as np
from math3d import (
    rand_unit_vector_cosine_weighted,
    plane_intersection,         # wafer plane
    sphere_intersection,        # shield surface 
    disk_hit,
)
from geometry import Scene

SPEED_MEAN = 8_000  # m/s, approximate orbital velocity

# ---------------------------------------------------------------------
# Particle source
# ---------------------------------------------------------------------
def sample_incident(scene: Scene, size: int, rng=None):
    """
    Sample origin & velocity vectors for 'size' particles.

    • Origins: flat disk at Z = +1 m, radius = 1.1 x shield.radius
    • Directions: cosine-weighted toward the NEGATIVE Z_axis
    """
    if rng is None:
        rng = np.random

    # disk sampling
    r_max = scene.shield.radius * 1.1       # every particle’s origin is picked on a disk that sits directly above the shield
    theta = rng.uniform(0, 2 * np.pi, size)
    radius = r_max * np.sqrt(rng.uniform(0, 1, size))
    x = radius * np.cos(theta)
    y = radius * np.sin(theta)
    z = np.full_like(x, 1.0)              # spawn the particles 1 meter above the shield plane

    pos = np.stack([x, y, z], axis=1)

    # cosine weighted directions toward –Z since that is the side facing the wafer
    # -Z == concave side, +Z == convex side
    velocity_dir = -rand_unit_vector_cosine_weighted(size)     # rand_unit_vector_cosine_weighted returns velocity vectors pointing straight up (+Z) so negate them to point (-Z)(size, 3)
    velocity   = velocity_dir * SPEED_MEAN                          # constant speed for all particles 

    return pos, velocity


# ---------------------------------------------------------------------
# Reflection helpers
# ---------------------------------------------------------------------
def reflect_specular(v_in: np.ndarray, normal: np.ndarray) -> np.ndarray:
    """Perfect mirror reflection."""
    return v_in - 2.0 * np.dot(v_in, normal) * normal


def reflect_diffuse(normal: np.ndarray, rng=None) -> np.ndarray:
    """Lambertian (cosine-weighted) hemisphere reflection."""
    if rng is None:
        rng = np.random

    local = rand_unit_vector_cosine_weighted(1)[0]      # in +Z hemi
    z_axis = np.array([0.0, 0.0, 1.0])

    # Rotate 'local' so that +Z aligns with 'normal'
    axis  = np.cross(z_axis, normal)
    axis_n = np.linalg.norm(axis)
    if axis_n < 1e-6:            # already aligned
        return local if normal[2] > 0 else -local

    axis /= axis_n
    angle = np.arccos(np.clip(np.dot(z_axis, normal), -1.0, 1.0))
    return rotate_vector(local, axis, angle)


def rotate_vector(v: np.ndarray, axis: np.ndarray, angle: float) -> np.ndarray:
    """Rodrigues rotation formula."""
    k = axis
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
    Trace 'batch_size' particles and return:

    • mean_deflection_deg : <θ> between –v_in and v_out (deg)
    • hit_ratio           : fraction that reach wafer
    """
    if rng is None:
        rng = np.random

    pos, vel = sample_incident(scene, batch_size, rng)

    wafer_hits      = 0
    total_theta_deg = 0.0

    # Spherical-cap geometry
    R_sphere   = 1.0 / scene.shield.curvature          # curvature = 1/R
    sphere_ctr = np.array([0.0, 0.0, R_sphere])        # cap centre

    for i in range(batch_size):
        v_in      = vel[i]
        v_in_unit = v_in / np.linalg.norm(v_in)

        # ---------- shield intersection (spherical cap) ----------
        hit = sphere_intersection(pos[i], v_in_unit, R_sphere)
        if np.any(np.isnan(hit)):
            continue                                   # missed shield

        # calculate the normal at which the particle hits the shield - Surface normal
        normal = (hit - sphere_ctr) / R_sphere        

        # ---------- reflection ----------
        if scene.shield.coating_type == "specular":
            v_out = reflect_specular(v_in_unit, normal)
        else:
            v_out = reflect_diffuse(normal, rng)

        # Deflection angle (deg) between –v_in and v_out
        cos_th  = np.clip(np.dot(-v_in_unit, v_out), -1.0, 1.0)
        theta   = np.degrees(np.arccos(cos_th))
        total_theta_deg += theta

        # ---------- wafer intersection ----------
        wafer_hit_pt = plane_intersection(hit, v_out, scene.wafer.z_offset)
        if np.any(np.isnan(wafer_hit_pt)):
            continue

        wafer_ctr = np.array(
            [scene.wafer.xy_offset[0], scene.wafer.xy_offset[1], scene.wafer.z_offset]
        )
        if disk_hit(wafer_hit_pt, wafer_ctr, scene.wafer.radius):
            wafer_hits += 1

    mean_deflection = total_theta_deg / batch_size
    hit_ratio       = wafer_hits / batch_size
    return mean_deflection, hit_ratio
