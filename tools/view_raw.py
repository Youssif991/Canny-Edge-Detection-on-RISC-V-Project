import sys
import os
import numpy as np
import matplotlib.pyplot as plt

## @brief Reads and displays a raw binary image file using Matplotlib.
#
#  If no GUI/Display is available (common in headless WSL2), it falls back
#  to saving a PNG version next to the raw file.
#
#  @param path The path to the .raw image file.
#  @param w    The width of the image in pixels. Defaults to 100.
#  @param h    The height of the image in pixels. Defaults to 75.
def show_raw(path, w=100, h=75):
    try:
        # Load the binary data and reshape into a 2D grid
        img = np.fromfile(path, dtype=np.uint8).reshape((h, w))
        
        plt.figure()
        plt.imshow(img, cmap='gray', vmin=0, vmax=255)
        plt.title(f"Raw Image: {os.path.basename(path)}")
        plt.axis('off')  # Hide axes for a cleaner look
        
        # Check backend to handle headless environments gracefully
        import matplotlib
        backend = matplotlib.get_backend().lower()
        non_gui_backends = {'agg', 'cairo', 'pdf', 'pgf', 'ps', 'svg', 'template'}
        
        if backend in non_gui_backends:
            # Fallback for headless environments (WSL2 without GUI forwarding)
            png_path = os.path.splitext(path)[0] + ".png"
            plt.savefig(png_path, bbox_inches='tight', pad_inches=0)
            plt.close()
            print(f"Non-interactive backend ({backend}) detected. Saved visualization to: {png_path}")
        else:
            plt.show()
            
    except FileNotFoundError:
        print(f"Error: File not found at {path}")
    except ValueError as e:
        print(f"Error: Could not reshape data. Ensure dimensions {w}x{h} are correct. {e}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python tools/view_raw.py [filename.raw] [width] [height]")
        print("Default size: 100x75")
        sys.exit(1)

    filename = sys.argv[1]
    
    # Construct the path to the assets folder relative to this script
    assets_dir = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../assets'))
    
    # Handle both direct paths and filename-only arguments
    if os.path.exists(filename):
        image_path = filename
    else:
        image_path = os.path.join(assets_dir, filename)

    # Automatically detect default dimensions if it matches our standard test size (7500 bytes)
    default_w, default_h = 100, 75
    if os.path.exists(image_path):
        file_size = os.path.getsize(image_path)
        if file_size == 7500:
            default_w, default_h = 100, 75
        elif file_size == 262144: # 512x512
            default_w, default_h = 512, 512

    try:
        image_width = int(sys.argv[2]) if len(sys.argv) > 2 else default_w
        image_height = int(sys.argv[3]) if len(sys.argv) > 3 else (int(sys.argv[2]) if len(sys.argv) > 2 else default_h)
    except ValueError:
        print("Invalid dimensions provided. Width and Height must be integers.")
        sys.exit(1)

    show_raw(image_path, image_width, image_height)
