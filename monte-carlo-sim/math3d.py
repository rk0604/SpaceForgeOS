"""
math3d.py — Low-level geometry/math utilities for the Monte Carlo tracer.

Functions:

- rand_unit_vector_cosine_weighted(n: int) -> np.ndarray:
    Returns `n` random 3D unit vectors, sampled from a cosine-weighted hemisphere
    around the Z+ axis. This models the real-world distribution of molecules hitting
    a surface — more arrive near normal incidence than glancing angles.

- plane_intersection(ray_origin: np.ndarray, ray_dir: np.ndarray, plane_z: float) -> np.ndarray:
    Given a ray starting at `ray_origin` and traveling along `ray_dir`, this function
    returns the 3D point at which the ray intersects a flat plane located at z = `plane_z`.
    Assumes ray_dir[2] != 0. If ray is parallel to the plane, return None or NaNs.

- disk_hit(point: np.ndarray, disk_center: np.ndarray, radius: float) -> bool:
    After a particle hits the plane, this function checks if it landed inside a
    circular disk (e.g. the wafer). Returns True if within radius; else False.
"""

import numpy as np



def rand_unit_vector_cosine_weighted(n: int) -> np.ndarray:
    """
    Generates `n` cosine-weighted random unit vectors pointing upward (Z+ hemisphere).
    Returns: (n, 3) array of vectors
    """
    # Uniform random values
    u1 = np.random.rand(n)
    u2 = np.random.rand(n)

    r = np.sqrt(u1)
    theta = 2 * np.pi * u2

    x = r * np.cos(theta)
    y = r * np.sin(theta)
    z = np.sqrt(1 - u1)  # ensures cosine-weighted distribution

    return np.stack((x, y, z), axis=1)  # shape: (n, 3)

# for treating the shield as a flat surface
def plane_intersection(ray_origin: np.ndarray, ray_dir: np.ndarray, plane_z: float) -> np.ndarray:
    """
    Returns intersection point of ray with plane z=plane_z.
    Returns NaNs if ray is parallel to the plane.
    
    Inputs:
        ray_origin: (3,) np.ndarray
        ray_dir: (3,) np.ndarray
        plane_z: float

    Output:
        point: (3,) np.ndarray (or np.nan if no intersection)
    """
    dz = ray_dir[2]
    if np.abs(dz) < 1e-8:
        return np.full(3, np.nan)  # No intersection (parallel)

    t = (plane_z - ray_origin[2]) / dz
    if t < 0:
        return np.full(3, np.nan)  # Intersection behind the ray start

    return ray_origin + t * ray_dir

# for treating the shield as a curved spherical surface
def sphere_intersection(o, d, R):
    # sphere centre at (0,0,R)
    c  = np.array([0.0, 0.0, R])
    oc = o - c
    b  = np.dot(d, oc)
    c2 = np.dot(oc, oc) - R*R
    disc = b*b - c2
    if disc < 0:
        return np.full(3, np.nan)          # no hit

    t = -b - np.sqrt(disc)                # nearer root
    if t < 0:
        return np.full(3, np.nan)
    return o + t*d

def disk_hit(point: np.ndarray, disk_center: np.ndarray, radius: float) -> bool:
    """
    Returns True if point lies within a radius of disk_center (XY plane).
    
    Inputs:
        point: (3,) np.ndarray — location of impact
        disk_center: (3,) np.ndarray — center of wafer (same z)
        radius: float — disk radius

    Output:
        bool — whether hit is inside wafer
    """
    dx = point[0] - disk_center[0]
    dy = point[1] - disk_center[1]
    dist2 = dx**2 + dy**2

    return dist2 <= radius**2

# whether the wake is interupted by the particles
def rays_hit_infinite_cone(o, d, apex, axis, cos2):
    """
    Return boolean array saying whether rays (o,d) intersect an infinite cone.

    Parameters
    ----------
    o : (N,3)  ray origins
    d : (N,3)  ray directions (unit length not required)
    apex : (3,) tip of cone
    axis : (3,) unit vector, cone axis (points downstream, -Z)
    cos2 : float,  cos²α   where α is the half-opening angle
    """
    ao   = o - apex                       # vector from apex to origin
    dv   = (d @ axis)                     # dot(d, axis)
    av   = (ao @ axis)                    # dot(ao, axis)

    # Quadratic coefficients for ‖ (ao + t d) × axis ‖² = (tanα)² ( (ao + t d)·axis )²
    # Simplifies to:  (dv² - cos2‖d‖²)·t² + 2(dv·av - cos2 d·ao)·t + (av² - cos2‖ao‖²) = 0
    d_norm2 = np.einsum("ij,ij->i", d, d)
    ao_norm2 = np.einsum("ij,ij->i", ao, ao)
    d_ao = np.einsum("ij,ij->i", d, ao)

    A = dv**2 - cos2 * d_norm2
    B = dv * av - cos2 * d_ao
    C = av**2 - cos2 * ao_norm2
    disc = B**2 - A * C

    # Hit if there is a real root with t ≥ 0 and ray is pointing downstream (dv < 0)
    # return (disc >= 0) & (dv < 0)
    return (disc >= 0) & (dv > 0)

# -------------------------------------------------------------- Vectorized versions of the functions 
# the vectorized verzion of sphere_intersection to accomodate larger batch sizes 
def sphere_intersection_batch(pos, v, R):
    # vectorized ray-sphere intersection with center at (0, 0, R)
    oc = pos - np.array([0, 0, R])
    b = np.sum(oc * v, axis=1)
    c = np.sum(oc ** 2, axis=1) - R ** 2
    discriminant = b ** 2 - c
    valid = discriminant >= 0

    t = np.where(valid, -b - np.sqrt(discriminant), np.nan)
    return pos + t[:, None] * v

# vectorized version of plane_intersection
def plane_intersection_batch(ray_origin: np.ndarray,
                             ray_dir:    np.ndarray,
                             plane_z:    float) -> np.ndarray:
    """
    Vectorized intersection of many rays with the plane z = plane_z.

    Parameters
    ----------
    ray_origin : (N, 3) ndarray
        Ray origins.
    ray_dir    : (N, 3) ndarray
        Ray directions.
    plane_z    : float
        Z-coordinate of the plane.

    Returns
    -------
    (N, 3) ndarray
        Intersection points; rows filled with NaNs where no intersection exists
        (ray parallel to plane or intersection behind origin).
    """
    dz = ray_dir[:, 2]
    parallel = np.abs(dz) < 1e-8                       # vectors ~‖ plane
    t = np.divide(plane_z - ray_origin[:, 2],
                  dz,
                  out=np.full_like(dz, np.nan),
                  where=~parallel)                     # avoid /0 for parallels
    behind = t < 0.0                                   # hits behind origin
    t[behind] = np.nan

    # Broadcast t to (N, 1) then add to origins
    return ray_origin + ray_dir * t[:, None]

# vectorized version of disk_hit
def disk_hit_batch(points:      np.ndarray,
                   disk_center: np.ndarray,
                   radius:      float) -> np.ndarray:
    """
    Boolean mask for whether each point lies inside a circular disk in the XY-plane.

    Parameters
    ----------
    points      : (N, 3) ndarray
    disk_center : (3,)   ndarray
    radius      : float

    Returns
    -------
    (N,) ndarray of bool
        True  ⇒ point is inside or on the rim
        False ⇒ point is outside
    """
    dx = points[:, 0] - disk_center[0]
    dy = points[:, 1] - disk_center[1]
    return dx**2 + dy**2 <= radius**2


