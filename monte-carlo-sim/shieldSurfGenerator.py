import math

def write_flat_disk_surf(filename, radius=1.0, n_segments=32, z=0.0):
    """Write a flat disk shield .surf file for SPARTA."""
    points = []
    tris = []
    
    # Center vertex
    points.append((0.0, 0.0, z))
    
    # Rim vertices
    for i in range(n_segments):
        theta = 2.0 * math.pi * i / n_segments
        x = radius * math.cos(theta)
        y = radius * math.sin(theta)
        points.append((x, y, z))
    
    # Triangles (fan from center)
    for i in range(1, n_segments):
        tris.append((1, i+1, i+2))
    tris.append((1, n_segments+1, 2))  # wrap around
    
    # Write to file
    with open(filename, "w") as f:
        f.write(f"POINTS {len(points)}\n")
        for x, y, zc in points:
            f.write(f"{x:.6f} {y:.6f} {zc:.6f}\n")
        f.write(f"TRIANGLES {len(tris)}\n")
        for a, b, c in tris:
            f.write(f"{a} {b} {c}\n")
    
    print(f"Wrote {filename} with {len(points)} points and {len(tris)} triangles.")

# Example usage:
write_flat_disk_surf("flat_disk.surf", radius=1.0, n_segments=32, z=0.0)
