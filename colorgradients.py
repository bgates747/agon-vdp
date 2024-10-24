import numpy as np
from PIL import Image
import colorsys
from agonimages import colors64hsv, findNearestColorHSV  # Import the required palettes and functions

def extract_unique_hue_saturation_and_value(palette):
    """Extract unique hue, saturation, and value values from the colors64hsv palette."""
    unique_hues = sorted(set([hsv[0] for hsv in palette]))
    return unique_hues

def generate_grid_for_hue(hue, sat_steps, val_steps, scale, numcolors=64):
    """Generate a grid image for a single hue using discrete saturation and value steps."""
    # Generate the discrete saturation and value steps
    saturation_values = np.linspace(1, 0, sat_steps)
    value_values = np.linspace(1, 0, val_steps)
    
    width = len(saturation_values) * scale
    height = len(value_values) * scale
    img = Image.new('RGB', (width, height))
    pixels = img.load()

    for y in range(height):
        for x in range(width):
            # Saturation decreases from left to right
            sat = saturation_values[x // scale]  # Saturation along the x-axis
            # Value decreases from top to bottom
            val = value_values[y // scale]       # Value along the y-axis
            hsv_color = (hue, sat, val)

            # Find the nearest Agon color in the HSV palette
            nearest_hsv = findNearestColorHSV(hsv_color, numcolors)

            # Convert nearest HSV color back to RGB for displaying
            r, g, b = colorsys.hsv_to_rgb(nearest_hsv[0], nearest_hsv[1], nearest_hsv[2])
            r, g, b = int(r * 255), int(g * 255), int(b * 255)

            # Set the pixel color
            pixels[x, y] = (r, g, b)

    return img

def generate_hue_grid_image(sat_steps, val_steps, scale, numcolors=64):
    """Generate a 4 x 6 grid of hue grids, sorted by hue."""
    # Extract the unique hues
    unique_hues = extract_unique_hue_saturation_and_value(colors64hsv)

    # Ensure there are exactly 24 hues
    assert len(unique_hues) == 24, "Expected 24 unique hues in the palette."

    # Size of each individual hue grid
    width_per_grid = sat_steps * scale
    height_per_grid = val_steps * scale

    # Total size of the full image
    total_width = grid_cols * width_per_grid
    total_height = grid_rows * height_per_grid

    # Create a blank image to hold the full grid
    full_img = Image.new('RGB', (total_width, total_height))

    # Loop through each hue and place it in the 6 x 4 grid
    for i, hue in enumerate(unique_hues):
        # Generate the grid for this hue
        hue_grid = generate_grid_for_hue(hue, sat_steps, val_steps, scale, numcolors)

        # Calculate the position in the 6 x 4 grid
        col = i % grid_cols
        row = i // grid_cols

        # Paste the hue grid into the full image
        x_offset = col * width_per_grid
        y_offset = row * height_per_grid
        full_img.paste(hue_grid, (x_offset, y_offset))

    # Print out the unique hue list for debugging
    print("Hue values:", unique_hues)

    # Show the full image
    full_img.show()

# Define the grid dimensions (6 x 4)
grid_cols = 6
grid_rows = 4
sat_steps = 1200 // grid_cols # Number of steps for saturation
val_steps = 800 // grid_rows  # Number of steps for value
scale = 1  # Scale factor for pixel size
generate_hue_grid_image(sat_steps, val_steps, scale)