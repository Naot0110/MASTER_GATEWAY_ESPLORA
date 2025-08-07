import tkinter as tk
from math import cos, sin, atan2, acos, sqrt, radians, degrees
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

# Chiều dài các đoạn
L1, L2, L3, L4 = 32.5, 90, 90, 19


class RobotArmApp:
    def __init__(self, root):
        self.root = root
        self.root.title("RRRR Robot Arm 3D Simulator")

        self.angles = [tk.DoubleVar(value=0) for _ in range(4)]
        self.coords = [tk.DoubleVar(value=0) for _ in range(4)]  # x, y, z, phi
        self.zoom_scale = 200  # Giá trị ban đầu cho scale trục XYZ

        control_frame = tk.Frame(root)
        control_frame.pack(side=tk.LEFT, padx=10, pady=10)

        for i in range(4):
            tk.Label(control_frame, text=f"Theta {i+1} (°)").grid(
                row=i, column=0, sticky="w"
            )
            s = tk.Scale(
                control_frame,
                variable=self.angles[i],
                from_=-180,
                to=180,
                orient=tk.HORIZONTAL,
                length=200,
                command=lambda e: self.update_fk(),
            )
            s.grid(row=i, column=1)

        for i, label in enumerate(["X", "Y", "Z", "Φ"]):
            tk.Label(control_frame, text=f"{label}_e:").grid(row=i, column=2)
            tk.Entry(control_frame, textvariable=self.coords[i], width=7).grid(
                row=i, column=3
            )

        tk.Button(control_frame, text="Update IK", command=self.update_ik).grid(
            row=5, column=1, columnspan=2, pady=5
        )
        tk.Button(control_frame, text="Zoom In", command=self.zoom_in).grid(
            row=5, column=2
        )
        tk.Button(control_frame, text="Zoom Out", command=self.zoom_out).grid(
            row=5, column=3
        )

        # Matplotlib 3D figure
        self.fig = plt.figure(figsize=(6, 6))
        self.ax = self.fig.add_subplot(111, projection="3d")
        self.canvas = FigureCanvasTkAgg(self.fig, master=root)
        self.canvas.get_tk_widget().pack(side=tk.RIGHT)

        self.update_fk()

    def dh_transform(self, a, alpha, d, theta):
        alpha, theta = radians(alpha), radians(theta)
        return np.array(
            [
                [
                    cos(theta),
                    -sin(theta) * cos(alpha),
                    sin(theta) * sin(alpha),
                    a * cos(theta),
                ],
                [
                    sin(theta),
                    cos(theta) * cos(alpha),
                    -cos(theta) * sin(alpha),
                    a * sin(theta),
                ],
                [0, sin(alpha), cos(alpha), d],
                [0, 0, 0, 1],
            ]
        )

    def forward_kinematics(self, theta_vals):
        T = np.eye(4)
        joints = [T[:3, 3].copy()]
        params = [
            (0, 90, L1, theta_vals[0]),
            (L2, 0, 0, theta_vals[1]),
            (L3, 0, 0, theta_vals[2]),
            (L4, 0, 0, theta_vals[3]),  # α = 0 thay vì 90
        ]
        for a, alpha, d, theta in params:
            T = T @ self.dh_transform(a, alpha, d, theta)
            joints.append(T[:3, 3].copy())
        return joints, T

    def update_fk(self):
        theta_vals = [v.get() for v in self.angles]
        joints, T = self.forward_kinematics(theta_vals)
        self.coords[0].set(round(T[0, 3], 2))
        self.coords[1].set(round(T[1, 3], 2))
        self.coords[2].set(round(T[2, 3], 2))
        self.coords[3].set(round(theta_vals[1] + theta_vals[2] + theta_vals[3], 2))
        self.plot_arm(joints)

    def update_ik(self):
        x_e, y_e, z_e, phi = [v.get() for v in self.coords]
        phi = radians(phi)
        theta1 = atan2(y_e, x_e)
        r = sqrt(x_e**2 + y_e**2)
        z = z_e - L1
        D = sqrt(r**2 + z**2)

        cos_theta3 = (D**2 - L2**2 - L3**2) / (2 * L2 * L3)
        if abs(cos_theta3) > 1:
            print("OUT OF RANGE")
            return

        theta3 = acos(cos_theta3)
        theta2 = atan2(z, r) - atan2(L3 * sin(theta3), L2 + L3 * cos(theta3))
        theta4 = phi - theta2 - theta3

        self.angles[0].set(degrees(theta1))
        self.angles[1].set(degrees(theta2))
        self.angles[2].set(degrees(theta3))
        self.angles[3].set(degrees(theta4))
        self.update_fk()

    def plot_arm(self, joints):
        self.ax.cla()
        s = self.zoom_scale
        self.ax.set_xlim(-s, s)
        self.ax.set_ylim(-s, s)
        self.ax.set_zlim(0, s * 1.5)
        self.ax.set_xlabel("X")
        self.ax.set_ylabel("Y")
        self.ax.set_zlabel("Z")

        xs, ys, zs = zip(*joints)
        self.ax.plot(xs, ys, zs, "-o", linewidth=3, color="lightblue")
        self.canvas.draw()

    def zoom_in(self):
        if self.zoom_scale > 50:
            self.zoom_scale -= 25
            self.update_fk()

    def zoom_out(self):
        if self.zoom_scale < 500:
            self.zoom_scale += 25
            self.update_fk()


if __name__ == "__main__":
    root = tk.Tk()
    app = RobotArmApp(root)
    root.mainloop()
