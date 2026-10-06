import numpy as np
import matplotlib.pyplot as plt

# Simulate 50ft green (approx 15m)
x = np.linspace(-7.5, 7.5, 100)
z = np.linspace(-7.5, 7.5, 100)
X, Z = np.meshgrid(x, z)

# Double breaker equation
Y = -0.05 * Z + 0.3 * np.sin(0.4 * Z) * np.cos(0.4 * X)

fig = plt.figure(figsize=(10, 8))
ax = fig.add_subplot(111, projection='3d')

# Plot the surface
surf = ax.plot_surface(X, Z, Y, cmap='viridis', alpha=0.8)

# Simulate putt trajectory (Euler integration)
# 10ft putt is about 3 meters. Let's start at Z = -4, X = 0
pos = np.array([0.0, -4.0])
vel = np.array([0.5, 3.5]) # slight angle
dt = 0.016
g = 9.81
mu = 0.05

trajectory_x = []
trajectory_z = []
trajectory_y = []

for _ in range(300):
    # Gradients for normal
    dx = -0.3 * 0.4 * np.sin(0.4 * pos[1]) * np.sin(0.4 * pos[0])
    dz = -0.05 + 0.3 * 0.4 * np.cos(0.4 * pos[1]) * np.cos(0.4 * pos[0])
    
    # Gravity break acceleration (tangent to surface)
    acc = np.array([-dx, -dz]) * g
    
    # Friction
    v_len = np.linalg.norm(vel)
    if v_len > 0.01:
        acc -= (vel / v_len) * (mu * g)
    else:
        vel = np.array([0.0, 0.0])
        
    vel += acc * dt
    pos += vel * dt
    
    trajectory_x.append(pos[0])
    trajectory_z.append(pos[1])
    # calculate y
    y = -0.05 * pos[1] + 0.3 * np.sin(0.4 * pos[1]) * np.cos(0.4 * pos[0])
    trajectory_y.append(y)
    
    if v_len <= 0.01:
        break

ax.plot(trajectory_x, trajectory_z, trajectory_y, color='red', linewidth=3, label='Putt Trajectory')
ax.scatter([trajectory_x[0]], [trajectory_z[0]], [trajectory_y[0]], color='white', s=50, label='Start')
ax.scatter([trajectory_x[-1]], [trajectory_z[-1]], [trajectory_y[-1]], color='black', s=50, label='End')

ax.set_xlabel('X (meters)')
ax.set_ylabel('Z (meters)')
ax.set_zlabel('Elevation (meters)')
ax.set_title('10ft Double-Breaker Putt Simulation')
ax.legend()

plt.savefig('/tmp/frame_final.png')
print("Saved /tmp/frame_final.png")
