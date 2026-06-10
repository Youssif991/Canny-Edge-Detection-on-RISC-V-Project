"""
gen_test_image.py — Generate known raw grayscale test images for the Canny pipeline.
Usage:
    python3 tools/gen_test_image.py              # default 512x512
    python3 tools/gen_test_image.py 100 75       # custom dimensions
    python3 tools/gen_test_image.py --test       # run built-in unit tests
Output: assets/*.raw
Author: Youssef
"""

import os
import sys
import numpy as np
import unittest

W, H = 512, 512  # non-power-of-two as required by hints guide

    ## @brief Initialize the generator with image dimensions.
    #  @param w Image width in pixels. Default: 512.
    #  @param h Image height in pixels. Default: 512.
    def __init__(self, w=512, h=512):
        self.w = w
        self.h = h

    ## @brief Create a blank black canvas.
    #  @return NumPy array of zeros with shape (h, w).
    def _blank(self):
        return np.zeros((self.h, self.w), dtype=np.uint8)

    ## @brief Save a NumPy array as a raw binary file in assets/.
    #  @param filename Output filename (e.g. "rect.raw").
    #  @param img      NumPy array to save.
    def _save(self, filename, img):
        assets_dir = os.path.abspath(
            os.path.join(os.path.dirname(os.path.abspath(__file__)), '../assets'))
        os.makedirs(assets_dir, exist_ok=True)
        filepath = os.path.join(assets_dir, filename)
        img.astype(np.uint8).tofile(filepath)
        print(f"Saved {filepath} ({self.w}x{self.h})")

    ## @brief White rectangle on black background.
    #  @details Tests that Gaussian blur softens rectangle edges.
    #  @return NumPy array with a white rectangle.
    def rect(self):
        img = self._blank()
        img[self.h//5 : 3*self.h//4,
            self.w//4 : 3*self.w//4] = 255
        return img

    ## @brief Vertical edge — left half black, right half white.
    #  @details Tests Sobel X: should produce large Gx, near-zero Gy.
    #  @return NumPy array with a vertical edge at the centre.
    def vertical_edge(self):
        img = self._blank()
        img[:, self.w//2:] = 255
        return img

    ## @brief Horizontal edge — top half black, bottom half white.
    #  @details Tests Sobel Y: should produce large Gy, near-zero Gx.
    #  @return NumPy array with a horizontal edge at the centre.
    def horizontal_edge(self):
        img = self._blank()
        img[self.h//2:, :] = 255
        return img

    ## @brief 45-degree diagonal edge.
    #  @details Tests direction quantization — expected direction: 45 degrees.
    #  @return NumPy array with a diagonal edge from top-left to bottom-right.
    def diagonal_edge(self):
        img = self._blank()
        for r in range(self.h):
            for c in range(self.w):
                if c > r * (self.w / self.h):
                    img[r, c] = 255
        return img

    ## @brief 135-degree inverse diagonal edge.
    #  @details Tests direction quantization — expected direction: 135 degrees.
    #  @return NumPy array with an inverse diagonal edge.
    def diagonal_edge_inv(self):
        img = self._blank()
        for r in range(self.h):
            for c in range(self.w):
                if c + r * (self.w / self.h) < self.w:
                    img[r, c] = 255
        return img

    ## @brief Filled white circle on black background.
    #  @details Tests non-axis-aligned edges — gradient direction rotates around circle.
    #  @return NumPy array with a white circle at the centre.
    def circle(self):
        img = self._blank()
        cy, cx = self.h // 2, self.w // 2
        radius = min(self.h, self.w) // 3
        Y, X = np.ogrid[:self.h, :self.w]
        mask = (X - cx)**2 + (Y - cy)**2 <= radius**2
        img[mask] = 255
        return img

    ## @brief Uniform mid-grey image (all pixels = 128).
    #  @details Gaussian blur of a uniform image must remain uniform (+-1 tolerance).
    #           Sobel gradient of a uniform image must be zero everywhere.
    #  @return NumPy array filled with 128.
    def uniform(self):
        return np.full((self.h, self.w), 128, dtype=np.uint8)

    ## @brief All-black image (all pixels = 0).
    #  @details Gaussian blur of all-black must remain all-black.
    #  @return NumPy array filled with 0.
    def full_black(self):
        return self._blank()

    ## @brief All-white image (all pixels = 255).
    #  @details Tests upper boundary — no overflow or clamping artifacts.
    #  @return NumPy array filled with 255.
    def full_white(self):
        return np.full((self.h, self.w), 255, dtype=np.uint8)

    ## @brief Generate and save all test images to assets/.
    def generate_all(self):
        images = {
            "rect.raw":              self.rect(),
            "vertical_edge.raw":     self.vertical_edge(),
            "horizontal_edge.raw":   self.horizontal_edge(),
            "diagonal_edge.raw":     self.diagonal_edge(),
            "diagonal_edge_inv.raw": self.diagonal_edge_inv(),
            "circle.raw":            self.circle(),
            "uniform.raw":           self.uniform(),
            "full_black.raw":        self.full_black(),
            "full_white.raw":        self.full_white(),
        }
        for filename, img in images.items():
            self._save(filename, img)
        print("Done.")


## @class TestTestImageGenerator
#  @brief Unit tests for TestImageGenerator.
class TestTestImageGenerator(unittest.TestCase):

    def setUp(self):
        self.gen = TestImageGenerator(w=512, h=512)

    def test_rect_has_white_centre(self):
        img = self.gen.rect()
        self.assertEqual(img.shape, (512, 512))
        self.assertEqual(img[256, 256], 255)  # inside rectangle
        self.assertEqual(img[0,   0],   0)    # outside rectangle

    def test_vertical_edge_left_black_right_white(self):
        img = self.gen.vertical_edge()
        self.assertEqual(img.shape, (512, 512))
        self.assertEqual(img[256, 50],  0)    # left half black
        self.assertEqual(img[256, 460], 255)  # right half white

    def test_horizontal_edge_top_black_bottom_white(self):
        img = self.gen.horizontal_edge()
        self.assertEqual(img.shape, (512, 512))
        self.assertEqual(img[50,  256], 0)    # top half black
        self.assertEqual(img[460, 256], 255)  # bottom half white

    def test_circle_centre_white_corner_black(self):
        img = self.gen.circle()
        self.assertEqual(img.shape, (512, 512))
        self.assertEqual(img[256, 256], 255)  # centre white
        self.assertEqual(img[0,   0],   0)    # corner black

    def test_uniform_all_128(self):
        img = self.gen.uniform()
        self.assertEqual(img.shape, (512, 512))
        self.assertTrue(np.all(img == 128))

    def test_full_black_all_zero(self):
        img = self.gen.full_black()
        self.assertEqual(img.shape, (512, 512))
        self.assertTrue(np.all(img == 0))

    def test_full_white_all_255(self):
        img = self.gen.full_white()
        self.assertEqual(img.shape, (512, 512))
        self.assertTrue(np.all(img == 255))

    def test_diagonal_edge_top_right_white(self):
        img = self.gen.diagonal_edge()
        self.assertEqual(img.shape, (512, 512))
        self.assertEqual(img[10, 500], 255)   # top-right white
        self.assertEqual(img[500, 10], 0)     # bottom-left black

    def test_diagonal_edge_inv_top_left_white(self):
        img = self.gen.diagonal_edge_inv()
        self.assertEqual(img.shape, (512, 512))
        self.assertEqual(img[10, 10],   255)  # top-left white
        self.assertEqual(img[500, 500], 0)    # bottom-right black

    def test_all_images_correct_shape(self):
        gen = self.gen
        for img in [gen.rect(), gen.vertical_edge(), gen.horizontal_edge(),
                    gen.diagonal_edge(), gen.diagonal_edge_inv(),
                    gen.circle(), gen.uniform(), gen.full_black(), gen.full_white()]:
            self.assertEqual(img.shape, (512, 512))


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--test":
        sys.argv.pop(1)
        unittest.main()
    else:
        try:
            w = int(sys.argv[1]) if len(sys.argv) > 1 else 512
            h = int(sys.argv[2]) if len(sys.argv) > 2 else 512
        except ValueError:
            print("Invalid dimensions. Usage: python3 tools/gen_test_image.py [width] [height]")
            sys.exit(1)

        gen = TestImageGenerator(w=w, h=h)
        gen.generate_all()