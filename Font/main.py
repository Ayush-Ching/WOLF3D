import cv2
import numpy as np
from PIL import Image
import os

# ----------------------------
# CONFIG
# ----------------------------
INPUT_IMAGE = "selected.png"        # your attached image
OUTPUT_DIR  = "glyphs"
MIN_AREA    = 30                # ignore tiny noise blobs

os.makedirs(OUTPUT_DIR, exist_ok=True)

# ----------------------------
# LOAD IMAGE
# ----------------------------
img = cv2.imread(INPUT_IMAGE, cv2.IMREAD_GRAYSCALE)

# Invert if needed (glyphs should be white)
# Assuming black glyphs on light background
_, binary = cv2.threshold(img, 50, 255, cv2.THRESH_BINARY_INV)

# ----------------------------
# FIND CONNECTED COMPONENTS
# ----------------------------
num_labels, labels, stats, _ = cv2.connectedComponentsWithStats(
    binary, connectivity=8
)

glyph_count = 0
for i in range(1, num_labels):
    x, y, w, h, area = stats[i]

    # Hard guards (VERY important)
    if area < MIN_AREA:
        continue
    if w <= 0 or h <= 0:
        continue

    glyph = binary[y:y+h, x:x+w]

    if glyph.size == 0:
        continue

    rgba = np.zeros((h, w, 4), dtype=np.uint8)
    rgba[..., 3] = glyph        # alpha
    rgba[..., 0:3] = 0        # white glyph
    glyph_count += 1
    filename = f"{i}.png"
    Image.fromarray(rgba).save(os.path.join(OUTPUT_DIR, filename))

print(f"Extracted {glyph_count} glyphs")
