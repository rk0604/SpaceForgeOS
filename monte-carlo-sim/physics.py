# type: ignore[index]
"""
physics.py - Monte-Carlo ray tracer for SpaceForge wake-shield study.
"""
import numpy as np

from math3d import (
    rand_unit_vector_cosine_weighted,
    plane_intersection,          # wafer plane hit (single)
    sphere_intersection,         # spherical shield hit (single)
    disk_hit,                    # point-in-wafer test (single)
    rays_hit_infinite_cone,      # analytic cone intersection (vectorised)
    sphere_intersection_batch,
    disk_hit_batch,
    plane_intersection_batch,
)
from geometry import Scene

# ---------------------------------------------------------------------------
# Physical constants for LEO molecular flux
# ---------------------------------------------------------------------------
K_B         = 1.380649e-23       # Boltzmann constant  [J/K]
# MASS_O2     = 2.656e-26          # kg  (molecular oxygen, dominant at 300 km)
T_EXO       = 1000.0             # K   (typical exospheric temperature)
ORBITAL_VEL = 7_700.0            # m/s relative flow speed at 300 km
ATT_JITTER_DEG = 0.3                     # 1 sigma attitude jitter (deg)
CROSSWIND = np.array([0.0, 0.0, 0.0])    # [u_east, u_north, 0] m/s


AMU = 1.66053906660e-27          # kg
SPECIES = [
    # name,  mass[kg],   T[K] ,  n[#/m^3]
    ("O",   16*AMU,     T_EXO,  5.0e8 * 1e6),   # 5*10^8 cm^-3 -> 5*10^14 m^-3
    ("O2",  32*AMU,     T_EXO,  1.2e7 * 1e6),   # 1.2*10^7 cm^-3 -> 1.2*10^13 m^-3
    ("N2",  28*AMU,     T_EXO,  1.0e8 * 1e6),           
    ("O+",  16*AMU,     T_EXO,  1.0e5 * 1e6),  
    ("O2+", 32*AMU,     T_EXO,  1.0e4 * 1e6),
]

# Pre-compute one-dimensional thermal speed scale
# V_TH = np.sqrt(K_B * T_EXO / MASS_O2)

# HELPER: for attitude jitter
def _jittered_flow_dirs(size: int, sigma_deg: float, rng) -> np.ndarray:
    """
    Return Nx3 unit vectors close to the ram direction (0,0,-1), with
    small Gaussian pitch/yaw jitter (sigma_deg).
    """
    sigma = np.deg2rad(sigma_deg)
    pitch = rng.normal(0.0, sigma, size)   # rotation about +X
    yaw   = rng.normal(0.0, sigma, size)   # rotation about +Y

    sx, cx = np.sin(pitch), np.cos(pitch)
    sy, cy = np.sin(yaw),   np.cos(yaw)

    # Apply R_x(pitch) · R_y(yaw) to v0 = (0,0,-1)
    # R_y(yaw) * v0 = (-sy, 0, -cy)
    vx = -sy
    vy =  sx * cy
    vz = -cx * cy

    v = np.stack([vx, vy, vz], axis=1)
    v /= np.linalg.norm(v, axis=1, keepdims=True)
    return v


# ---------------------------------------------------------------------------
# Helper: ray–square-pyramid intersection (vectorised)
# ---------------------------------------------------------------------------
def pyramid_intersection_batch(o: np.ndarray,
                               d: np.ndarray,
                               h: float,
                               half_base: float):
    """
    Vectorised intersection of many rays with a square pyramid whose
    apex is at (0, 0, 0) and whose base lies in z = -h.

    The four plane equations are:
        -x + z*slope = 0
         x + z*slope = 0
        -y + z*slope = 0
         y + z*slope = 0
    where slope = half_base / h.
    """
    slope  = half_base / h
    planes = [(-1.0,  0.0,  slope, 0.0),
              ( 1.0,  0.0,  slope, 0.0),
              ( 0.0, -1.0,  slope, 0.0),
              ( 0.0,  1.0,  slope, 0.0)]

    hits     = np.full(o.shape, np.nan)
    normals  = np.full(o.shape, np.nan)
    face_sel = np.full(o.shape[0], -1)  # which plane wins for each ray

    for idx, (a, b, c, d0) in enumerate(planes):
        denom = a * d[:, 0] + b * d[:, 1] + c * d[:, 2]
        valid = np.abs(denom) > 1e-8
        t     = (d0 - (a * o[:, 0] + b * o[:, 1] + c * o[:, 2])) / denom
        cond  = valid & (t > 0.0)

        p       = o + t[:, None] * d
        inside  = (np.abs(p[:, 0]) <= half_base) & \
                  (np.abs(p[:, 1]) <= half_base) & \
                  (p[:, 2] <= 0.0)

        choose = cond & inside & (
            (face_sel == -1) | (t < np.linalg.norm(hits - o, axis=1))
        )
        if not choose.any():
            continue

        hits[choose]     = p[choose]
        face_sel[choose] = idx

    # outward normals (pointing upstream, +Z)
    base_norms = np.array([
        [-slope,  0.0,  1.0],
        [ slope,  0.0,  1.0],
        [ 0.0,  -slope, 1.0],
        [ 0.0,   slope, 1.0]
    ])
    base_norms /= np.linalg.norm(base_norms, axis=1, keepdims=True)

    valid_idx = face_sel >= 0
    normals[valid_idx] = base_norms[face_sel[valid_idx]]

    return hits, normals


def _j5_planes(a: float):
    """
    Outward face normals n and plane offsets d (n·p = d) for a Johnson J5 cupola.
    We model the 5 square walls + 5 triangular walls.  The tiny roof pentagon is
    ignored, and the base decagon is treated as perfectly absorbing (so its plane
    is not included).

            5 top-ring vertices  ── Vp  (pentagon,  z = 0)
           10 bottom-ring verts  ── Vd  (decagon,   z = -a)

    Vertex indices used below:
        0-4 : top pentagon
        5-14: decagon (two per square, CCW)
    """

    # 1. key radii / heights -----------------------------------------
    r_p = a / (2 * np.sin(np.pi / 5))       # circ. radius of top pentagon
    r_d = a / (2 * np.sin(np.pi / 10))      # circ. radius of base decagon
    z_top, z_bot = 0.0, -a                  # squares are one edge-length tall

    # 2. vertex rings -------------------------------------------------
    phi_p = np.linspace(np.pi/2, 2*np.pi + np.pi/2, 5, endpoint=False)
    Vp = np.c_[r_p * np.cos(phi_p),
               r_p * np.sin(phi_p),
               np.full(5, z_top)]

    # two decagon vertices per square, at ±18° around each pentagon vertex
    phi_d = phi_p[:, None] + np.deg2rad([-18, +18])
    Vd = np.c_[r_d * np.cos(phi_d.ravel()),
              r_d * np.sin(phi_d.ravel()),
              np.full(10, z_bot)]

    verts = np.vstack([Vp, Vd])             # 15 vertices total

    # 3. faces --------------------------------------------------------
    # squares: top edge on pentagon, bottom edge on decagon
    squares = [[i,
                (i + 1) % 5,
                5 + (2*i + 1) % 10,
                5 + 2*i]                    # bottom uses matching decagon edge
               for i in range(5)]

    # triangles between squares
    triangles = [[i,
                  5 + 2*i,
                  5 + (2*i + 1) % 10]
                 for i in range(5)]

    faces = squares + triangles

    # 4. plane equations n·p = d -------------------------------------
    n, d = [], []
    for f in faces:
        p0, p1, p2 = verts[f[:3]]
        normal = np.cross(p1 - p0, p2 - p0)
        normal /= np.linalg.norm(normal)
        # Make sure the normal points upstream (+Z)
        if normal[2] < 0:
            normal = -normal
        n.append(normal)
        d.append(np.dot(normal, p0))

    return np.stack(n), np.array(d)


def cupola_intersection_batch(o, d, edge_len):
    n, d0 = _j5_planes(edge_len)
    Np, Nf = o.shape[0], n.shape[0]
    hits    = np.full((Np,3), np.nan)
    normals = np.full((Np,3), np.nan)
    t_best  = np.full(Np, np.inf)

    for k in range(Nf):
        denom  = d @ n[k]
        mask   = np.abs(denom) > 1e-8
        t      = (d0[k] - o @ n[k]) / denom
        mask  &= t > 0
        p      = o + t[:,None]*d
        inside = np.all((p @ n.T) - d0 <= 1e-8, axis=1)   # every face
        choose = mask & inside & (t < t_best)
        t_best[choose] = t[choose]
        hits[choose]   = p[choose]
        normals[choose]= n[k]            # constant over that face
    return hits, normals

# ---------------------------------------------------------------------------
# Particle source
# ---------------------------------------------------------------------------
'''
def sample_incident(scene: Scene, size: int, rng=None):
    """Generate particle origins and velocities upstream of the shield."""
    rng = np.random if rng is None else rng

    # Origins on a disc one metre above the apex
    r_max = scene.shield.primary_dim * 1.2
    theta = rng.uniform(0.0, 2.0 * np.pi, size)
    r     = r_max * np.sqrt(rng.uniform(0.0, 1.0, size))

    x = r * np.cos(theta)
    y = r * np.sin(theta)
    z = np.full_like(x, 1.0)  # z = +1 m
    pos = np.stack([x, y, z], axis=1)

    # Velocity = bulk drift toward –Z plus thermal spread
    v_dir   = -rand_unit_vector_cosine_weighted(size)
    v_drift = v_dir * ORBITAL_VEL
    thermal = rng.normal(0.0, V_TH, (size, 3))
    vel     = v_drift + thermal

    return pos, vel
'''

# New sample incident with multispecies and attitude jitter
def sample_incident(scene: Scene, size: int, rng=None):
    """Generate particle origins and velocities upstream of the shield."""
    rng = np.random if rng is None else rng

    # ---- origins on a disc above the apex (unchanged logic)
    r_max = scene.shield.primary_dim * 1.2
    theta = rng.uniform(0.0, 2.0*np.pi, size)
    r     = r_max * np.sqrt(rng.uniform(0.0, 1.0, size))
    x = r * np.cos(theta)
    y = r * np.sin(theta)
    z = np.full_like(x, 1.0)   # z = +1 m upstream
    pos = np.stack([x, y, z], axis=1)

    # ---- (7) attitude jitter around ram direction (0,0,-1)
    flow_dirs = _jittered_flow_dirs(size, ATT_JITTER_DEG, rng)

    # ---- optional crosswind (set to zeros if not used)
    v_drift = flow_dirs * ORBITAL_VEL + CROSSWIND

    # ---- (3) multi-species thermal spread: per-particle sigma = sqrt(kT/m)
    names, masses, temps, n_m3 = zip(*SPECIES)
    n_m3 = np.array(n_m3, dtype=float)
    p_species = n_m3 / n_m3.sum()               # probability ∝ number density

    sp_idx = rng.choice(len(SPECIES), size=size, p=p_species)

    m_arr = np.asarray(masses)[sp_idx]
    T_arr = np.asarray(temps)[sp_idx]
    sigma = np.sqrt(K_B * T_arr / m_arr)        # 1-D thermal σ [m/s]

    thermal = rng.normal(0.0, 1.0, (size, 3)) * sigma[:, None]
    vel = v_drift + thermal

    return pos, vel, sp_idx


# ---------------------------------------------------------------------------
# Main batch tracer
# ---------------------------------------------------------------------------
def trace_batch(scene: Scene, batch_size: int, rng=None) -> tuple[float, float, float, float, np.ndarray]:
    """
    Trace batch_size particles through one Scene.

    Returns
    -------
    mean_deflection_deg : float
    hit_ratio           : float
    wake_intrusion_ratio: float
    wafer_flux_m2s : float  # particles per m^2 per s at the wafer  # speed- and species-weighted flux proxy (lower is better)
    """
    rng = np.random if rng is None else rng

    # start each batch with a clean wafer grid -----------------------
    # This mutates the array in-place (allowed even with frozen dataclass).
    scene.wafer.grid.fill(0.0)
    grid_snapshot = None  # will be set after binning; fallback to zeros if no hits
    # -------------------------------------------------------------------------

    # 1) initial sample (now expects species index too)
    pos, vel, sp_idx = sample_incident(scene, batch_size, rng)

    # keep speed magnitudes for weighting; use unit dirs for geometry
    speed = np.linalg.norm(vel, axis=1)
    # guard against any zero-speed entries
    speed = np.where(speed > 0, speed, 1e-12)
    v_in_unit = vel / speed[:, None]

    # 2) shield intersection
    if scene.shield.profile == "cap":
        R_sphere = 1.0 / scene.shield.shape_param
        if scene.shield.primary_dim > R_sphere:
            R_sphere = scene.shield.primary_dim * 0.99
        sphere_ctr = np.array([0.0, 0.0, -R_sphere])
        hit_pts = sphere_intersection_batch(pos, v_in_unit, -R_sphere)
        normals_all = (hit_pts - sphere_ctr) / R_sphere

    elif scene.shield.profile == "flat":
        hit_pts = plane_intersection_batch(pos, v_in_unit, plane_z=0.0)
        normals_all = np.tile(np.array([0.0, 0.0, 1.0]), (batch_size, 1))

    elif scene.shield.profile == "pyramid":
        h_py      = scene.shield.primary_dim * max(scene.shield.shape_param, 0.3)
        half_base = scene.shield.primary_dim
        hit_pts, normals_all = pyramid_intersection_batch(pos, v_in_unit, h_py, half_base)

    elif scene.shield.profile == "cupola":
        hit_pts, normals_all = cupola_intersection_batch(pos, v_in_unit, edge_len=scene.shield.primary_dim)

    else:
        raise ValueError(f"Unknown shield profile '{scene.shield.profile}'")

    # clip hits outside disk rim (cap/flat only)
    if scene.shield.profile in ("cap", "flat"):
        r2 = hit_pts[:, 0]**2 + hit_pts[:, 1]**2
        rim_mask = r2 <= scene.shield.primary_dim**2
        hit_pts[~rim_mask]     = np.nan
        normals_all[~rim_mask] = np.nan

    miss_mask = np.isnan(hit_pts).any(axis=1)
    hit_mask  = ~miss_mask

    # 3) outgoing rays
    ray_pos = np.where(miss_mask[:, None], pos, hit_pts)
    ray_dir = np.empty_like(vel)
    defl_deg = np.full(batch_size, 180.0)  # misses keep default

    # A) miss
    ray_dir[miss_mask] = v_in_unit[miss_mask]

    # B) hit
    if hit_mask.any():
        normals = normals_all[hit_mask]

        if scene.shield.coating_type == "specular":
            dot_nv = np.einsum("ij,ij->i", v_in_unit[hit_mask], normals)
            v_out  = v_in_unit[hit_mask] - 2.0 * dot_nv[:, None] * normals
            
        else:
            local_dirs = rand_unit_vector_cosine_weighted(normals.shape[0])
            z_axis     = np.array([0.0, 0.0, 1.0])
            axes       = np.cross(np.broadcast_to(z_axis, normals.shape), normals)
            axes_norm  = np.linalg.norm(axes, axis=1, keepdims=True)
            cosA       = normals[:, 2]
            sinA       = np.sqrt(np.clip(1.0 - cosA**2, 0.0, 1.0))
            k_unit     = np.divide(axes, axes_norm, out=np.zeros_like(axes), where=axes_norm > 1e-8)
            term1      = local_dirs * cosA[:, None]
            term2      = np.cross(k_unit, local_dirs) * sinA[:, None]
            term3_scale= np.einsum("ij,ij->i", k_unit, local_dirs)
            term3      = k_unit * term3_scale[:, None] * (1.0 - cosA)[:, None]
            v_out      = term1 + term2 + term3
            aligned    = axes_norm[:, 0] < 1e-8
            v_out[aligned & (normals[:, 2] < 0)] *= -1.0

        ray_dir[hit_mask] = v_out
        cos_th = np.einsum("ij,ij->i", -v_in_unit[hit_mask], v_out)
        defl_deg[hit_mask] = np.degrees(np.arccos(np.clip(cos_th, -1.0, 1.0)))

    mean_deflection_deg = defl_deg[hit_mask].mean() if hit_mask.any() else 0.0

    # 4) wake intrusion test
    if hasattr(scene.wake, "axis") and hasattr(scene.wake, "cos2"):
        wake_mask = rays_hit_infinite_cone(ray_pos, ray_dir, apex=np.zeros(3),
                                           axis=scene.wake.axis, cos2=scene.wake.cos2)
    else:
        slope = scene.wake.half_base / scene.wake.length
        cos2_alpha = 1.0 / (1.0 + slope**2)
        wake_mask = rays_hit_infinite_cone(ray_pos, ray_dir, apex=np.zeros(3),
                                           axis=np.array([0.0, 0.0, -1.0]), cos2=cos2_alpha)

    # 5) wafer hit test
    wafer_ctr = np.array([*scene.wafer.xy_offset, scene.wafer.z_offset])
    wafer_pts = plane_intersection_batch(ray_pos, ray_dir, scene.wafer.z_offset)
    wafer_hit_mask = (~np.isnan(wafer_pts).any(axis=1)) & disk_hit_batch(wafer_pts, wafer_ctr, scene.wafer.radius)
    
    # bin wafer hits into the wafer grid
    if wafer_hit_mask.any():
        # positions relative to wafer center (xy only)
        R = scene.wafer.radius
        xy_rel = wafer_pts[wafer_hit_mask, :2] - wafer_ctr[:2]   # shape (H, 2)

        # normalize from [-R, R] -> [0, 1] then to integer indices [0, N-1]
        # rows correspond to y, cols to x (row-major)
        H, W = scene.wafer.grid.shape
        u = (xy_rel[:, 0] / R + 1.0) * 0.5   # x in [0,1]
        v = (xy_rel[:, 1] / R + 1.0) * 0.5   # y in [0,1]

        cols = np.clip((u * W).astype(int), 0, W - 1)
        rows = np.clip((v * H).astype(int), 0, H - 1)

        # increment counts (unweighted)
        np.add.at(scene.wafer.grid, (rows, cols), 1.0)

        # If you want cosine-weighted binning instead, uncomment:
        # cos_inc = np.clip(-ray_dir[wafer_hit_mask, 2], 0.0, None)
        # np.add.at(scene.wafer.grid, (rows, cols), cos_inc)

        # --- NEW: snapshot after binning so caller can persist it -------------
        grid_snapshot = scene.wafer.grid.copy()
        # ---------------------------------------------------------------------

    # ---------------------------------------------------------------------------

    wake_hits  = np.count_nonzero(wake_mask | wafer_hit_mask)
    wafer_hits = np.count_nonzero(wafer_hit_mask)

    # ----------- convert to physical flux on wafer [1 / (m^2 s)] -----------
    # 1) upstream directed flux per unit area (sum over species)
    #    For LEO ram, drift dominates, so use ORBITAL_VEL as v_rel for all species.
    #    If you want to include crosswind later, use np.linalg.norm(ORBITAL_VEL*z_hat + CROSSWIND).
    n_species = np.array([s[3] for s in SPECIES], dtype=float)     # [#/m^3]
    v_rel = np.linalg.norm(np.array([0.0, 0.0, ORBITAL_VEL]) + CROSSWIND)  # m/s
    F0 = (n_species.sum()) * v_rel                                  # [#/m^2 s] hitting a flat upstream plane

    # 2) how many real particles per second does ONE launched sample represent?
    #    You launch particles uniformly on a disc of radius r_max = shield.primary_dim * 1.2 (same as in sample_incident).
    r_max = scene.shield.primary_dim * 1.2
    A_src = np.pi * r_max * r_max                                   # source disc area [m^2]
    R_per_sample = (F0 * A_src) / float(batch_size)                  # [#/s] represented by one simulated particle

    # 3) from simulated samples, how many hit the wafer per second?
    #    Weight each hit by cos(incidence) so glancing hits contribute less area-normal flux.
    wafer_area = np.pi * scene.wafer.radius * scene.wafer.radius     # [m^2]
    if wafer_hits > 0:
        cos_inc = np.clip(-ray_dir[wafer_hit_mask, 2], 0.0, None)    # |v_hat * n_hat|, wafer normal is +Z
        hits_per_sec = R_per_sample * cos_inc.sum()                   # [#/s] onto the whole wafer
        wafer_flux_m2s = hits_per_sec / wafer_area                    # [#/m^2 s]
    else:
        wafer_flux_m2s = 0.0

    # --- NEW: ensure grid_snapshot is always defined -------------------------
    if grid_snapshot is None:
        # No wafer hits this batch; return the (zero) grid state.
        grid_snapshot = scene.wafer.grid.copy()
    # -------------------------------------------------------------------------

    return (
        mean_deflection_deg,
        wafer_hits / batch_size,
        wake_hits  / batch_size,
        wafer_flux_m2s,   # physical: particles per m^2 per second at wafer
        grid_snapshot,
    )
