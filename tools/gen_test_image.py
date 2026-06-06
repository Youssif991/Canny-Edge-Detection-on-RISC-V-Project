"""
gen_test_image.py — Generate known raw grayscale test images.
Usage: python3 tools/gen_test_image.py
Output: assets/*.raw
"""

import numpy as np
import os

W, H = 100, 75  # non-power-of-two as required by hints guide

os.makedirs("assets", exist_ok=True)

def save_raw(filename, img):
    img.astype(np.uint8).tofile(f"assets/{filename}")
    print(f"Saved assets/{filename} ({W}x{H})")

# White rectangle on black background
rect = np.zeros((H, W), dtype=np.uint8)
rect[20:55, 25:75] = 255
save_raw("rect.raw", rect)

# Vertical edge (left=black, right=white) — tests Sobel X
vedge = np.zeros((H, W), dtype=np.uint8)
vedge[:, W//2:] = 255
save_raw("vertical_edge.raw", vedge)

# Horizontal edge (top=black, bottom=white) — tests Sobel Y
hedge = np.zeros((H, W), dtype=np.uint8)
hedge[H//2:, :] = 255
save_raw("horizontal_edge.raw", hedge)

# Diagonal edge — tests direction quantization
diag = np.zeros((H, W), dtype=np.uint8)
for r in range(H):
    for c in range(W):
        if c > r * (W / H):
            diag[r, c] = 255
save_raw("diagonal_edge.raw", diag)

# Circle — tests non-axis-aligned edges
circle = np.zeros((H, W), dtype=np.uint8)
cy, cx = H // 2, W // 2
for r in range(H):
    for c in range(W):
        if (r - cy)**2 + (c - cx)**2 < (min(H, W) // 3)**2:
            circle[r, c] = 255
save_raw("circle.raw", circle)

# Uniform image — blur should leave it unchanged
uniform = np.full((H, W), 128, dtype=np.uint8)
save_raw("uniform.raw", uniform)

print("Done.")
