import math

size_ft = 50.0
size_m = size_ft * 0.3048
resolution = 100
half_size = size_m / 2.0
step = size_m / resolution

def get_y(px, pz):
    return -0.05 * pz + 0.1 * math.sin(0.4 * pz) * math.cos(0.4 * px)

def get_normal(px, pz):
    d = 0.01
    dx = get_y(px + d, pz) - get_y(px - d, pz)
    dz = get_y(px, pz + d) - get_y(px, pz - d)
    # cross product of (2d, dx, 0) and (0, dz, 2d)
    # (-dx*2d, 4d^2, -dz*2d)
    nx = -dx * 2*d
    ny = 4 * d * d
    nz = -dz * 2*d
    l = math.sqrt(nx*nx + ny*ny + nz*nz)
    return (nx/l, ny/l, nz/l)

with open('assets/green.obj', 'w') as f:
    f.write("o Green\n")
    # Generate vertices
    for z in range(resolution + 1):
        for x in range(resolution + 1):
            px = -half_size + x * step
            pz = -half_size + z * step
            y = get_y(px, pz)
            f.write(f"v {px:.4f} {y:.4f} {pz:.4f}\n")
    
    # Generate faces
    v_offset = 0
    for z in range(resolution):
        for x in range(resolution):
            top_left = v_offset + z * (resolution + 1) + x + 1
            top_right = top_left + 1
            bottom_left = v_offset + (z + 1) * (resolution + 1) + x + 1
            bottom_right = bottom_left + 1
            f.write(f"f {top_left} {bottom_left} {top_right}\n")
            f.write(f"f {top_right} {bottom_left} {bottom_right}\n")
            
    # Generate ball trajectory (golf ball is 42.67mm diameter -> r=0.021335)
    r = 0.021335 * 4.0 # Scale up visually so it's visible
    px, pz = 2.0, -5.0
    vx, vz = -0.6, 2.5
    
    v_offset = (resolution + 1) * (resolution + 1)
    
    for i in range(120): # 120 frames
        # simple physics
        dt = 1.0 / 60.0
        y = get_y(px, pz)
        nx, ny, nz = get_normal(px, pz)
        
        # apply gravity down the slope
        gx = nx * 9.81 * dt
        gz = nz * 9.81 * dt
        
        vx += gx
        vz += gz
        
        # friction
        vx *= 0.98
        vz *= 0.98
        
        px += vx * dt
        pz += vz * dt
        
        if px < -half_size or px > half_size or pz < -half_size or pz > half_size:
            break
            
        # Draw sphere at px, y+r, pz
        # Generate simple icosphere or just a cube for the trail
        # Cube is much easier
        cx = px
        cy = y + r
        cz = pz
        
        # 8 vertices
        f.write(f"v {cx-r:.4f} {cy-r:.4f} {cz-r:.4f}\n")
        f.write(f"v {cx+r:.4f} {cy-r:.4f} {cz-r:.4f}\n")
        f.write(f"v {cx+r:.4f} {cy+r:.4f} {cz-r:.4f}\n")
        f.write(f"v {cx-r:.4f} {cy+r:.4f} {cz-r:.4f}\n")
        f.write(f"v {cx-r:.4f} {cy-r:.4f} {cz+r:.4f}\n")
        f.write(f"v {cx+r:.4f} {cy-r:.4f} {cz+r:.4f}\n")
        f.write(f"v {cx+r:.4f} {cy+r:.4f} {cz+r:.4f}\n")
        f.write(f"v {cx-r:.4f} {cy+r:.4f} {cz+r:.4f}\n")
        
        # 6 faces
        b = v_offset + 1
        f.write(f"f {b} {b+1} {b+2}\n")
        f.write(f"f {b+2} {b+3} {b}\n")
        f.write(f"f {b+1} {b+5} {b+6}\n")
        f.write(f"f {b+6} {b+2} {b+1}\n")
        f.write(f"f {b+5} {b+4} {b+7}\n")
        f.write(f"f {b+7} {b+6} {b+5}\n")
        f.write(f"f {b+4} {b} {b+3}\n")
        f.write(f"f {b+3} {b+7} {b+4}\n")
        f.write(f"f {b+3} {b+2} {b+6}\n")
        f.write(f"f {b+6} {b+7} {b+3}\n")
        f.write(f"f {b+4} {b+5} {b+1}\n")
        f.write(f"f {b+1} {b} {b+4}\n")
        
        v_offset += 8

print("Generated assets/green.obj with ball trajectory")
