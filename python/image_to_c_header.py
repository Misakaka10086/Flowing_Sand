import os
from PIL import Image, UnidentifiedImageError

# --- Configuration ---
FRAME_WIDTH = 16
FRAME_HEIGHT = 16
FRAME_PIXELS = FRAME_WIDTH * FRAME_HEIGHT
FRAME_BYTES = FRAME_PIXELS * 3


# --- Helper Function to format byte data for C array ---
def format_bytes_for_c(
    byte_list, bytes_per_line=16 * 3
):  # Changed default to 16 pixels * 3 bytes
    """Formats a list of bytes into a C-style array initializer string."""
    lines = []
    for i in range(0, len(byte_list), bytes_per_line):
        line_bytes = byte_list[i : i + bytes_per_line]
        hex_bytes = [f"0x{b:02X}" for b in line_bytes]
        lines.append(", ".join(hex_bytes))
    return ",\n  ".join(lines)


# --- Main Conversion Function ---
def convert_image_to_header(image_path: str, output_header_name: str):
    """
    Converts an image (static or GIF) to a C header file for a 16x16 WS2812 matrix.
    Transparent pixels are converted to black (RGB 0,0,0).

    Args:
        image_path: Path to the input image file (.png, .jpg, .gif, etc.).
        output_header_name: Name of the output header file (e.g., "my_animation").
                            The output file will be named my_animation.h and
                            C variables will be named accordingly (my_animation_frames, etc.).
    """
    if not os.path.exists(image_path):
        print(f"Error: Image file not found at '{image_path}'")
        return

    output_filename = f"{output_header_name}.h"
    # Sanitize for C variable name: replace non-alphanumeric chars with underscore
    anim_variable_name = "".join(e if e.isalnum() else "_" for e in output_header_name)
    # Ensure it doesn't start with a digit (valid C identifier)
    if anim_variable_name[0].isdigit():
        anim_variable_name = "_" + anim_variable_name

    frame_data_list = []  # List to hold byte data for each frame

    try:
        img = Image.open(image_path)

        is_animated = hasattr(img, "is_animated") and img.is_animated
        num_frames = img.n_frames if is_animated else 1

        print(
            f"Processing '{os.path.basename(image_path)}' ({'GIF' if is_animated else 'Static'} with {num_frames} frames)..."
        )

        for frame_index in range(num_frames):
            if is_animated:
                # Seek to the frame for GIF
                img.seek(frame_index)

            # --- Handle Transparency: Convert to RGBA and composite onto black ---
            # Ensure the frame has an alpha channel (even if it's full opacity)
            frame_rgba = img.convert("RGBA")

            # Create a black background image matching the current frame's size
            # We use the frame's *original* size before resizing to 16x16
            background = Image.new("RGB", frame_rgba.size, color="black")

            # Paste the RGBA frame onto the black background.
            # The alpha channel of frame_rgba is used as the mask.
            # Where alpha is 0, the black background shows through.
            # Where alpha is 255, the original pixel color covers the background.
            # Where alpha is between 0 and 255, colors are blended.
            background.paste(frame_rgba, (0, 0), frame_rgba)
            # Now 'background' is an RGB image where transparent areas from the source are black

            # --- Resize the composited image to the target dimensions ---
            # Use LANCZOS for high-quality downsampling
            resized_frame = background.resize(
                (FRAME_WIDTH, FRAME_HEIGHT), Image.Resampling.LANCZOS
            )
            # --- End Transparency Handling and Resizing ---

            # Extract pixel data (getdata returns a sequence of (R, G, B) tuples)
            pixel_data = list(resized_frame.getdata())

            # Flatten the pixel data into a byte list [R1, G1, B1, R2, G2, B2, ...]
            flat_bytes = []
            for r, g, b in pixel_data:
                flat_bytes.extend([r, g, b])

            if len(flat_bytes) != FRAME_BYTES:
                print(
                    f"Warning: Frame {frame_index} data length mismatch after processing! Expected {FRAME_BYTES}, got {len(flat_bytes)}"
                )
                # This should not happen with correct resize and mode conversion, but good to check.

            frame_data_list.append(flat_bytes)
            print(f"  Processed frame {frame_index + 1}/{num_frames}")

    except FileNotFoundError:
        print(f"Error: File not found at {image_path}")
        return
    except UnidentifiedImageError:
        print(
            f"Error: Could not open or read image file '{image_path}'. Unsupported format?"
        )
        return
    except Exception as e:
        print(f"An unexpected error occurred during image processing: {e}")
        import traceback

        traceback.print_exc()  # Print traceback for debugging
        return

    if not frame_data_list:
        print(
            f"Error: No frame data was successfully processed from '{image_path}'. Aborting header generation."
        )
        return

    # --- Generate C Header Content ---
    header_content = f"""\
#pragma once

#include <stdint.h>

// Animation data generated from {os.path.basename(image_path)}
// Transparent pixels were converted to black.

// Frame dimensions
#define FRAME_WIDTH {FRAME_WIDTH}
#define FRAME_HEIGHT {FRAME_HEIGHT}
#define FRAME_PIXELS (FRAME_WIDTH * FRAME_HEIGHT)
#define FRAME_BYTES (FRAME_PIXELS * 3)

// Structure to hold animation metadata
typedef struct {{
  const uint8_t* const* frames;  // Pointer to an array of frame pointers
  const uint16_t frameCount;
}} Animation;

"""
    # Note the double curly braces {{ and }} to escape literal braces in f-strings
    # The f-string syntax error happened here:
    # header_content += f"};\n" <--- Incorrect
    # header_content += f"}};\n" <--- Corrected

    # Add frame data arrays
    for i, frame_data in enumerate(frame_data_list):
        header_content += f"// Frame {i}\n"
        header_content += (
            f"const uint8_t {anim_variable_name}_frame{i}[FRAME_BYTES] = {{\n  "
        )
        header_content += format_bytes_for_c(frame_data)  # Use the updated helper
        header_content += f"\n}}; // End of frame {i}\n\n"  # Corrected f-string syntax

    # Add array of frame pointers
    frame_pointer_list = [
        f"{anim_variable_name}_frame{i}" for i in range(len(frame_data_list))
    ]
    header_content += f"// Array of pointers to each frame's data\n"
    header_content += f"const uint8_t* const {anim_variable_name}_frames[{len(frame_data_list)}] = {{\n  "  # Specify size
    header_content += ",\n  ".join(frame_pointer_list)
    header_content += (
        f"\n}}; // End of frame pointers array\n\n"  # Corrected f-string syntax
    )

    # Add the main Animation struct instance
    header_content += f"// Animation definition\n"
    # Use double braces for the struct initializer syntax
    header_content += f"const Animation {anim_variable_name}_anim = {{\n"
    header_content += f"  {anim_variable_name}_frames,\n"
    header_content += f"  sizeof({anim_variable_name}_frames) / sizeof({anim_variable_name}_frames[0])\n"
    header_content += (
        f"}}; // End of Animation struct instance\n"  # Corrected f-string syntax
    )

    # --- Write to file ---
    try:
        with open(output_filename, "w") as f:
            f.write(header_content)
        print(
            f"Successfully generated '{output_filename}' ({len(frame_data_list)} frames)"
        )
    except IOError as e:
        print(f"Error writing to file '{output_filename}': {e}")
    except Exception as e:
        print(f"An unexpected error occurred during file writing: {e}")
        import traceback

        traceback.print_exc()


# --- Example Usage ---
if __name__ == "__main__":
    # --- Requirements ---
    # You need to install the Pillow library:
    # pip install Pillow

    # --- How to use ---
    # 1. Save this script as, e.g., `img_to_header.py`
    # 2. Make sure you have Pillow installed (`pip install Pillow`)
    # 3. Run from your terminal:
    #    python img_to_header.py

    # Example: Convert a GIF animation (replace 'path/to/your/animation.gif' with a real path)
    print("\n--- Converting GIF Animation ---")
    gif_path = "python/mario_walk__r1351314241.gif"  # <<< CHANGE THIS PATH
    output_gif_name = "animated_heart"

    if gif_path and os.path.exists(gif_path):
        convert_image_to_header(gif_path, output_gif_name)
        print(f"Look for '{output_gif_name}.h'")
    else:
        print("Skipping GIF conversion due to missing file.")

    print("\nScript finished.")
