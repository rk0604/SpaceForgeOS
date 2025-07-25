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


    