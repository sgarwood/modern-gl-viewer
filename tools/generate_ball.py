import math

r = 0.021335
resolution = 16

with open('assets/ball.obj', 'w') as f:
    f.write("o Ball\n")
    for i in range(resolution + 1):
        lat = math.pi * i / resolution
        z = r * math.cos(lat)
        r_xy = r * math.sin(lat)
        for j in range(resolution + 1):
            lon = 2 * math.pi * j / resolution
            x = r_xy * math.cos(lon)
            y = r_xy * math.sin(lon)
            f.write(f"v {x:.6f} {y:.6f} {z:.6f}\n")
            # Normals
            f.write(f"vn {x/r:.6f} {y/r:.6f} {z/r:.6f}\n")
            
    # Faces
    for i in range(resolution):
        for j in range(resolution):
            first = (i * (resolution + 1)) + j + 1
            second = first + (resolution + 1)
            
            f.write(f"f {first}//{first} {second}//{second} {first+1}//{first+1}\n")
            f.write(f"f {second}//{second} {second+1}//{second+1} {first+1}//{first+1}\n")
