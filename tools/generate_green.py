import math

size_ft = 50.0
size_m = size_ft * 0.3048
resolution = 50
half_size = size_m / 2.0
step = size_m / resolution

with open('assets/green.obj', 'w') as f:
    f.write("o Green\n")
    # Generate vertices
    for z in range(resolution + 1):
        for x in range(resolution + 1):
            px = -half_size + x * step
            pz = -half_size + z * step
            
            # Double breaker equation: sine wave in X, cosine wave in Z
            # Or a simple polynomial:
            # Let's make a surface that slopes down slightly, but wiggles left and right.
            # Y = slope_z * pz + amplitude * sin(freq * pz) * cos(freq * px)
            
            y = -0.1 * pz + 0.5 * math.sin(0.5 * pz) * math.cos(0.5 * px)
            
            f.write(f"v {px:.4f} {y:.4f} {pz:.4f}\n")
    
    # Generate faces
    for z in range(resolution):
        for x in range(resolution):
            # 1-indexed
            top_left = z * (resolution + 1) + x + 1
            top_right = top_left + 1
            bottom_left = (z + 1) * (resolution + 1) + x + 1
            bottom_right = bottom_left + 1
            
            f.write(f"f {top_left} {bottom_left} {top_right}\n")
            f.write(f"f {top_right} {bottom_left} {bottom_right}\n")
            
print("Generated assets/green.obj")
