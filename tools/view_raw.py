import sys
import os
import numpy as np
import matplotlib.pyplot as plt

## @brief Reads and displays a raw binary image file using Matplotlib.
def show_raw(path, w=100, h=75, is_16bit=False):
    try:
        # Load the binary data using the dynamic type
        pixel_type = np.int16 if is_16bit else np.uint8
        img = np.fromfile(path, dtype=pixel_type).reshape((h, w))
        
        plt.figure()
        
        # For 16-bit images, let matplotlib auto-scale the contrast
        if is_16bit:
            plt.imshow(img, cmap='gray') 
        else:
            plt.imshow(img, cmap='gray', vmin=0, vmax=255)
            
        plt.title(f"Raw Image: {os.path.basename(path)}")
        plt.axis('off')  
        
        import matplotlib
        backend = matplotlib.get_backend().lower()
        non_gui_backends = {'agg', 'cairo', 'pdf', 'pgf', 'ps', 'svg', 'template'}
        
        if backend in non_gui_backends:
            png_path = os.path.splitext(path)[0] + ".png"
            plt.savefig(png_path, bbox_inches='tight', pad_inches=0)
            plt.close()
            print(f"Non-interactive backend ({backend}) detected. Saved visualization to: {png_path}")
        else:
            plt.show()
            
    except FileNotFoundError:
        print(f"Error: File not found at {path}")
    except ValueError as e:
        print(f"Error: Could not reshape data. Ensure dimensions {w}x{h} and data type match. {e}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python tools/view_raw.py [filename.raw] [width] [height]")
        sys.exit(1)

    filename = sys.argv[1]
    assets_dir = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../assets'))
    
    if os.path.exists(filename):
        image_path = filename
    else:
        image_path = os.path.join(assets_dir, filename)

    # Smart Auto-detection
    default_w, default_h = 100, 75
    is_16bit = False

    if os.path.exists(image_path):
        file_size = os.path.getsize(image_path)
        
        if file_size == 7500:             # Old test size (100x75 uint8)
            default_w, default_h = 100, 75
            is_16bit = False
        elif file_size == 15000:          # New signed 16-bit size (100x75 int16_t = 15000 bytes)
            default_w, default_h = 100, 75
            is_16bit = True
        elif file_size == 262144:         # Input Image size (512x512 uint8)
            default_w, default_h = 512, 512
            is_16bit = False
        elif file_size == 524288:         # Sobel Output size (512x512 int16_t)
            default_w, default_h = 512, 512
            is_16bit = True
        elif file_size == 409600:         # 640x640 Image size (uint8)
            default_w, default_h = 640, 640
            is_16bit = False
        elif file_size == 819200:         # 640x640 Output size (int16_t)
            default_w, default_h = 640, 640
            is_16bit = True
        elif file_size == 1920000:        # 1200x1600 Image size (uint8)
            default_w, default_h = 1200, 1600
            is_16bit = False
        elif file_size == 3840000:        # 1200x1600 Output size (int16_t)
            default_w, default_h = 1200, 1600
            is_16bit = True
        elif file_size == 2073600:        # 1080p Image size (1920x1080 uint8)
            default_w, default_h = 1920, 1080
            is_16bit = False
        elif file_size == 4147200:        # 1080p Output size (1920x1080 int16_t)
            default_w, default_h = 1920, 1080
            is_16bit = True

    try:
        image_width = int(sys.argv[2]) if len(sys.argv) > 2 else default_w
        image_height = int(sys.argv[3]) if len(sys.argv) > 3 else default_h
    except ValueError:
        print("Invalid dimensions provided. Width and Height must be integers.")
        sys.exit(1)

    # Pass down configuration flags
    show_raw(image_path, image_width, image_height, is_16bit)