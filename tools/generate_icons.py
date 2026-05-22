#!/usr/bin/env python3
"""Generate placeholder app icons for Kinema."""

import os
import sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS_DIR = os.path.join(PROJECT_ROOT, "assets")


def create_placeholder(size):
    """Create a placeholder icon at the given size."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Background gradient (dark blue/purple to orange)
    for y in range(size):
        ratio = y / size
        r = int(45 + (230 - 45) * ratio)
        g = int(55 + (130 - 55) * ratio)
        b = int(72 + (60 - 72) * ratio)
        draw.line([(0, y), (size, y)], fill=(r, g, b))

    # Rounded rectangle mask
    radius = size // 8
    mask = Image.new("L", (size, size), 0)
    mask_draw = ImageDraw.Draw(mask)
    mask_draw.rounded_rectangle(
        [(0, 0), (size - 1, size - 1)],
        radius=radius,
        fill=255
    )

    # Apply mask to background
    bg = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    bg.paste(img, mask=mask)
    img = bg
    draw = ImageDraw.Draw(img)

    # Draw "K" letter
    font_size = int(size * 0.55)
    try:
        font = ImageFont.truetype("/System/Library/Fonts/Helvetica.ttc", font_size)
    except:
        try:
            font = ImageFont.truetype("/System/Library/Fonts/HelveticaNeue.ttc", font_size)
        except:
            font = ImageFont.load_default()

    text = "K"
    bbox = draw.textbbox((0, 0), text, font=font)
    text_width = bbox[2] - bbox[0]
    text_height = bbox[3] - bbox[1]
    x = (size - text_width) // 2
    y = (size - text_height) // 2 - int(size * 0.05)

    # Text shadow
    shadow_offset = max(1, size // 64)
    draw.text(
        (x + shadow_offset, y + shadow_offset),
        text,
        font=font,
        fill=(0, 0, 0, 128)
    )

    # Text
    draw.text(
        (x, y),
        text,
        font=font,
        fill=(255, 255, 255, 255)
    )

    return img


def generate_macos_iconset():
    """Generate macOS .icns file."""
    iconset_dir = os.path.join(ASSETS_DIR, "icon.iconset")
    os.makedirs(iconset_dir, exist_ok=True)

    sizes = [16, 32, 64, 128, 256, 512, 1024]
    for size in sizes:
        img = create_placeholder(size)
        img.save(os.path.join(iconset_dir, f"icon_{size}x{size}.png"))
        # Also create @2x versions for some sizes
        if size <= 512:
            img2x = create_placeholder(size * 2)
            img2x.save(os.path.join(iconset_dir, f"icon_{size}x{size}@2x.png"))

    icns_path = os.path.join(ASSETS_DIR, "icon.icns")
    result = os.system(f"iconutil -c icns '{iconset_dir}' -o '{icns_path}'")
    if result != 0:
        print("WARNING: iconutil failed, .icns not created")
        return None

    print(f"Created: {icns_path}")
    return icns_path


def generate_windows_ico():
    """Generate Windows .ico file."""
    sizes = [16, 24, 32, 48, 64, 128, 256]
    images = [create_placeholder(s) for s in sizes]

    ico_path = os.path.join(ASSETS_DIR, "icon.ico")
    images[0].save(
        ico_path,
        format="ICO",
        sizes=[(s, s) for s in sizes],
        append_images=images[1:]
    )

    print(f"Created: {ico_path}")
    return ico_path


def main():
    os.makedirs(ASSETS_DIR, exist_ok=True)

    print("Generating placeholder icons for Kinema...")
    print("")

    icns = generate_macos_iconset()
    ico = generate_windows_ico()

    print("")
    print("Done! Replace these files with your real icons later:")
    if icns:
        print(f"  macOS: {icns}")
    print(f"  Windows: {ico}")
    print(f"  Icon set folder: {os.path.join(ASSETS_DIR, 'icon.iconset')}")


if __name__ == "__main__":
    main()
