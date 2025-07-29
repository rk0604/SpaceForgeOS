'''
1. Shield():
     - returns: radius, curvature, thickness, mass, coating_type
     - Purpose: central tuneable entity
2. WaferPlane():
     - returns: radius = 0.15m (150mm)
     - Target to be tested for hits - ideally 0
3. Scene():
     - returns: shield and wafer
     - purpose: bundle every static parameter for one simulation run
'''

from dataclasses import dataclass
import math
import numpy as np

@dataclass(frozen=True)
class Shield:
     radius:float   # in meters
     curvature: float  # describes the "concaveness" of the shield
     thickness: float  # used for mass calculation
     coating_type: str  # "specular" or "diffuse" - controls reflection behavior in physics loop
     
     def area(self) -> float:
          """Surface area assuming a circular disk."""
          return (math.pi * self.radius ** 2)

     def mass(self, density: float = 2700) -> float:
        """
        Estimate mass using area, thickness, and material density.
        Default density is aluminum (kg/m^3).
        """
        return self.area() * self.thickness * density
   

@dataclass(frozen=True)
class WaferPlane:
    radius: float      # meters (usually 0.15 for 300 mm)
    z_offset: float    # how far behind shield (e.g. 0.1 m)
    xy_offset: tuple   # (x, y) misalignment tolerance (default to (0.0, 0.0))


@dataclass(frozen=True)
class WakeCone:
    half_angle_deg: float        # alpha  (half–opening angle)
    length: float                # how far downstream you care to monitor [m]

     # returns the unit vector along the cone's central axis - scene.wake.axis
    @property
    def axis(self) -> np.ndarray:        # global –Z by convention so the cone points opposite of the negative velocity
        return np.array([0.0, 0.0, -1.0])

     # used repeatedly for intersection test 
    @property
    def cos2(self) -> float:             # cos^2(alpha) (pre-computed)
        return 1.0 / (1.0 + np.tan(np.radians(self.half_angle_deg))**2)
   
   
@dataclass(frozen=True)
class Scene:
    shield: Shield
    wafer: WaferPlane
    wake:  WakeCone
    
'''
Sample usage:
scene = Scene(
    shield=Shield(...),
    wafer=WaferPlane(...)
)
mean_deflection, hit_ratio = trace_particles(scene, n_particles=1_000_000)
'''

