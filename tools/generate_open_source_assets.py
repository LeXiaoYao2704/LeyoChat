"""Generate LeyoChat-owned media without external artwork or fonts.

All pixels are drawn from geometric primitives with deterministic parameters.
Run this script from any directory; outputs are written relative to the
repository root.
"""

from __future__ import annotations

import math
import random
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
BRANDING = ROOT / "resources" / "branding"
STICKERS = ROOT / "stickers" / "default"
ELA_IMAGES = ROOT / "third_party" / "ElaWidgetTools" / "Image"


def lerp(a: int, b: int, amount: float) -> int:
    return round(a + (b - a) * amount)


def animated_background(path: Path, dark: bool) -> None:
    width, height = 800, 500
    frames: list[Image.Image] = []
    top = (16, 25, 38) if dark else (225, 242, 250)
    bottom = (35, 54, 66) if dark else (247, 250, 248)
    line = (82, 196, 180) if dark else (34, 126, 164)
    accent = (236, 190, 96) if dark else (225, 112, 86)

    for frame_index in range(24):
        image = Image.new("RGB", (width, height))
        draw = ImageDraw.Draw(image)
        for y in range(height):
            amount = y / (height - 1)
            color = tuple(lerp(top[i], bottom[i], amount) for i in range(3))
            draw.line((0, y, width, y), fill=color)

        phase = frame_index * math.tau / 24
        for band in range(7):
            points = []
            baseline = 105 + band * 52
            for x in range(-20, width + 21, 8):
                wave = math.sin(x / 88 + phase + band * 0.7) * (15 + band * 1.5)
                points.append((x, round(baseline + wave)))
            color = line if band % 2 == 0 else accent
            alpha = 150 - band * 12
            overlay = Image.new("RGBA", image.size, (0, 0, 0, 0))
            ImageDraw.Draw(overlay).line(points, fill=(*color, alpha), width=2)
            image = Image.alpha_composite(image.convert("RGBA"), overlay).convert("RGB")

        dot_draw = ImageDraw.Draw(image)
        for dot in range(28):
            x = (dot * 97 + frame_index * (3 + dot % 4)) % width
            y = 45 + (dot * 61) % 390
            radius = 1 + dot % 3
            dot_draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=accent)
        frames.append(image)

    frames[0].save(
        path,
        save_all=True,
        append_images=frames[1:],
        duration=100,
        loop=0,
        optimize=True,
        disposal=2,
    )


def draw_face(draw: ImageDraw.ImageDraw, name: str, offset: int) -> None:
    outline = (34, 38, 42)
    fill = (250, 202, 72)
    draw.ellipse((18, 16 + offset, 102, 100 + offset), fill=fill, outline=outline, width=4)

    if name == "cool":
        draw.rounded_rectangle((32, 43 + offset, 55, 56 + offset), 3, fill=outline)
        draw.rounded_rectangle((65, 43 + offset, 88, 56 + offset), 3, fill=outline)
        draw.line((55, 48 + offset, 65, 48 + offset), fill=outline, width=4)
    elif name == "shock":
        draw.ellipse((34, 42 + offset, 45, 55 + offset), fill=outline)
        draw.ellipse((75, 42 + offset, 86, 55 + offset), fill=outline)
        draw.ellipse((51, 65 + offset, 69, 87 + offset), outline=outline, width=4)
        return
    elif name == "angry":
        draw.line((31, 43 + offset, 48, 49 + offset), fill=outline, width=4)
        draw.line((72, 49 + offset, 89, 43 + offset), fill=outline, width=4)
        draw.ellipse((38, 50 + offset, 46, 58 + offset), fill=outline)
        draw.ellipse((74, 50 + offset, 82, 58 + offset), fill=outline)
    elif name == "think":
        draw.ellipse((37, 47 + offset, 46, 56 + offset), fill=outline)
        draw.line((70, 48 + offset, 85, 45 + offset), fill=outline, width=4)
    elif name in {"sad", "cry"}:
        draw.line((34, 51 + offset, 46, 48 + offset), fill=outline, width=4)
        draw.line((74, 48 + offset, 86, 51 + offset), fill=outline, width=4)
        if name == "cry":
            draw.ellipse((39, 56 + offset, 47, 72 + offset), fill=(74, 162, 224))
            draw.ellipse((73, 56 + offset, 81, 72 + offset), fill=(74, 162, 224))
    else:
        draw.arc((31, 42 + offset, 49, 58 + offset), 15, 165, fill=outline, width=4)
        draw.arc((71, 42 + offset, 89, 58 + offset), 15, 165, fill=outline, width=4)

    if name in {"sad", "cry", "angry"}:
        draw.arc((44, 70 + offset, 76, 91 + offset), 200, 340, fill=outline, width=4)
    elif name == "think":
        draw.line((48, 79 + offset, 71, 77 + offset), fill=outline, width=4)
        draw.ellipse((83, 78 + offset, 101, 96 + offset), fill=(238, 174, 67), outline=outline, width=3)
    else:
        draw.arc((42, 62 + offset, 78, 88 + offset), 5, 175, fill=outline, width=4)


def sticker_frame(name: str, frame_index: int) -> Image.Image:
    image = Image.new("RGBA", (120, 120), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    offset = round(math.sin(frame_index * math.tau / 8) * 2)

    if name == "heart":
        pulse = 2 if frame_index in {2, 3} else 0
        draw.polygon(
            [(60, 101 + pulse), (22 - pulse, 61), (22, 38), (35, 25),
             (51, 25), (60, 35), (69, 25), (85, 25), (98, 38), (98 + pulse, 61)],
            fill=(226, 70, 89),
            outline=(120, 35, 50),
        )
    elif name in {"thumbsup", "wave", "clap", "ok"}:
        skin = (246, 194, 117)
        outline = (55, 48, 43)
        if name == "ok":
            draw.ellipse((28, 35 + offset, 70, 77 + offset), outline=outline, width=9)
            draw.line((62, 67 + offset, 91, 97 + offset), fill=outline, width=10)
            draw.line((68, 57 + offset, 96, 75 + offset), fill=skin, width=12)
        else:
            tilt = frame_index % 4 - 2 if name == "wave" else 0
            draw.rounded_rectangle((43 + tilt, 44 + offset, 83 + tilt, 101 + offset), 12,
                                   fill=skin, outline=outline, width=4)
            for finger in range(4):
                x = 34 + finger * 14 + tilt
                draw.rounded_rectangle((x, 20 + offset, x + 12, 65 + offset), 6,
                                       fill=skin, outline=outline, width=3)
            if name == "thumbsup":
                draw.polygon([(43, 64), (20, 61), (16, 48), (24, 36), (48, 56)],
                             fill=skin, outline=outline)
            elif name == "clap":
                draw.line((20, 35, 10, 23), fill=(237, 151, 59), width=4)
                draw.line((94, 35, 108, 24), fill=(237, 151, 59), width=4)
    else:
        draw_face(draw, name, offset)

    return image


def animated_sticker(path: Path, name: str) -> None:
    frames = [sticker_frame(name, index) for index in range(8)]
    frames[0].save(
        path,
        save_all=True,
        append_images=frames[1:],
        duration=110,
        loop=0,
        optimize=True,
        disposal=2,
        transparency=0,
    )


def ela_placeholders() -> None:
    avatar = Image.new("RGB", (256, 256), (224, 235, 240))
    draw = ImageDraw.Draw(avatar)
    draw.ellipse((78, 42, 178, 142), fill=(75, 126, 152))
    draw.rounded_rectangle((48, 145, 208, 245), 55, fill=(75, 126, 152))
    avatar.save(ELA_IMAGES / "DefaultAvatar.jpg", quality=92)

    scene = Image.new("RGB", (512, 320), (28, 42, 58))
    draw = ImageDraw.Draw(scene)
    draw.ellipse((80, 36, 250, 206), fill=(231, 215, 145))
    draw.ellipse((135, 15, 290, 180), fill=(28, 42, 58))
    draw.line((0, 245, 512, 245), fill=(70, 142, 153), width=4)
    scene.save(ELA_IMAGES / "DefaultScene.jpg", quality=92)

    mica = Image.new("RGB", (512, 512))
    draw = ImageDraw.Draw(mica)
    for y in range(512):
        amount = y / 511
        draw.line((0, y, 512, y), fill=(lerp(226, 244, amount), lerp(238, 246, amount), lerp(248, 242, amount)))
    mica.save(ELA_IMAGES / "MicaBase.png", optimize=True)

    randomizer = random.Random(0x1E70)
    noise = Image.new("L", (128, 128))
    noise.putdata([randomizer.randrange(96, 161) for _ in range(128 * 128)])
    noise.save(ELA_IMAGES / "noise.png", optimize=True)


def main() -> None:
    BRANDING.mkdir(parents=True, exist_ok=True)
    STICKERS.mkdir(parents=True, exist_ok=True)
    ELA_IMAGES.mkdir(parents=True, exist_ok=True)

    animated_background(BRANDING / "window-movie-light.gif", dark=False)
    animated_background(BRANDING / "window-movie-dark.gif", dark=True)
    for name in ("laugh", "cry", "thumbsup", "heart", "clap", "wave",
                 "think", "angry", "cool", "sad", "shock", "ok"):
        animated_sticker(STICKERS / f"{name}.gif", name)
    ela_placeholders()


if __name__ == "__main__":
    main()
