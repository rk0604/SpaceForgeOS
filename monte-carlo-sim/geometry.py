'''
1. Shield():
     - returns: radius, shape_param, thickness, mass, coating_type
     - Purpose: central tuneable entity
2. WaferPlane():
     - returns: radius = 0.15m (150mm)
     - Target to be tested for hits - ideally 0
3. Scene():
     - returns: shield and wafer
     - purpose: bundle every static parameter for one simulation run
'''

from dataclasses import dataclass, field
import math
import numpy as np
from typing import Union

J5_SQ   = 5
J5_TRI  = 5
J5_DEC  = 1              # decagon base
J5_PENT = 1              # top pentagon  (optional)

SQ_AREA   = lambda a: a * a
TRI_AREA  = lambda a: (np.sqrt(3) / 4) * a * a
DEC_AREA  = lambda a: 2.5 / np.tan(np.pi / 10) * a * a
PENT_AREA = lambda a: 0.25 * np.sqrt(25 + 10*np.sqrt(5)) * a * a   # exact

# ------------------------- Shield -------------------------
@dataclass(frozen=True)
class Shield:
    primary_dim:  float          # edge length (cupola) or radius/half-base
    shape_param:  float
    thickness:    float
    coating_type: str
    profile:      str = "cap"
    include_base: bool = True    #
    include_top:  bool = False   # tiny roof; toggle if you want mass margin

    # ---------- surface area ----------
    def area(self) -> float:
        if self.profile == "pyramid":
            h = max(self.shape_param, 0.3) * self.primary_dim
            a = self.primary_dim
            slant = math.hypot(a, h)
            return 4 * a * slant

        if self.profile == "cap":
            R = 1.0 / self.shape_param
            h = R - math.sqrt(max(R*R - self.primary_dim**2, 0.0))
            return 2 * math.pi * R * h

        if self.profile == "cupola":
            a = self.primary_dim
            area = (J5_SQ  * SQ_AREA(a) +
                    J5_TRI * TRI_AREA(a))
            if self.include_base:
                area += J5_DEC  * DEC_AREA(a)
            if self.include_top:
                area += J5_PENT * PENT_AREA(a)
            return area

        # flat disk
        return math.pi * self.primary_dim**2

    # ---------- mass ----------
    def mass(self, density: float = 2700) -> float:
        return self.area() * self.thickness * density


@dataclass(frozen=True)
class WaferPlane:
    radius: float      # meters (usually 0.15 for 300 mm)
    z_offset: float    # how far behind shield (e.g. 0.1 m)
    xy_offset: tuple   # (x, y) misalignment tolerance
    grid: np.ndarray = field(default_factory=lambda: np.zeros((50, 50), dtype=float))  # setup the grid to track where the wafers hit


@dataclass(frozen=True)
class WakeCone:
    half_angle_deg: float        # alpha  (half–opening angle)
    length: float                # how far downstream you care to monitor [m]

    # returns the unit vector along the cone's central axis - scene.wake.axis
    @property
    def axis(self) -> np.ndarray:        # global –Z by convention so the cone points downstream
        return np.array([0.0, 0.0, -1.0])

    # used repeatedly for intersection test 
    @property
    def cos2(self) -> float:             # cos²(alpha) (pre-computed)
        return 1.0 / (1.0 + np.tan(np.radians(self.half_angle_deg))**2)
    
    
@dataclass(frozen=True)
class PyramidWake:
    """
    Wake volume shaped as a square pyramid whose apex coincides with the
    shield apex and whose faces extend downstream with the same slope as
    the shield faces.  Useful for pyramid-profile shields.
    
        apex        : (0,0,0)  - same as shield apex
        half_base   : float    - half-width of the square cross-section at z = length
        length      : float    - how far downstream to monitor   (m)
    """
    half_base: float
    length: float

    # 1. Face normals  (same four planes as shield, but with -z so they extend downstream)
    def face_normals(self, shield_height: float) -> np.ndarray:
        slope = self.half_base / self.length          # tan(θ) of wake faces
        normals = np.array([
            [-slope,  0.0,  1.0],     # -x face   (points outward)
            [ slope,  0.0,  1.0],     #  x face
            [ 0.0 , -slope, 1.0],     # -y face
            [ 0.0 ,  slope, 1.0]      #  y face
        ])
        return normals / np.linalg.norm(normals, axis=1, keepdims=True)

    # 2. Fast point-inside test  (vectorised)
    def contains(self, pts: np.ndarray, shield_height: float) -> np.ndarray:
        """
        pts : (N,3) array of XYZ points in global coords (shield apex at origin,
              +Z upstream, -Z downstream)
        Returns a boolean mask of length N: True if point is inside the wake.
        """
        slope = self.half_base / self.length
        x, y, z = pts[:,0], pts[:,1], pts[:,2]
        # All points must be downstream (z <= 0) and not behind the truncated length
        valid_z = (z <= 0.0) & (z >= -self.length)
        # Check against each of the four planes  |x| + slope·z <= half_base
        inside  = (
            ( -x + slope*(-z) <= self.half_base ) &
            (  x + slope*(-z) <= self.half_base ) &
            ( -y + slope*(-z) <= self.half_base ) &
            (  y + slope*(-z) <= self.half_base )
        )
        return valid_z & inside



# ------------------------------------------------------ Main Scene -----------------------------------------------------------------------------------
@dataclass(frozen=True)
class Scene:
    shield: Shield
    wafer: WaferPlane
    wake:  Union[WakeCone, PyramidWake]

'''
Sample usage:
scene = Scene(
    shield=Shield(...),
    wafer=WaferPlane(...),
    wake =WakeCone(...)
)
mean_deflection, hit_ratio = trace_batch(scene, n_particles=1_000_000)
'''
