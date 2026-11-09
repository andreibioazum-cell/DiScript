"""Draws the five gift stickers as PNG sprites (flat cartoon style).

Run from the repo root: python3 tools/gen_gift_textures.py
Every sticker is authored on a 256x256 grid, drawn 4x and downsampled to
192x192, which the gift cards show at about 64 px.
"""
from PIL import Image, ImageDraw
import os

DESIGN = 256       # every sticker is authored on a 256x256 grid
S = 4              # supersampling
N = 192            # final size: cards draw the icon at ~64 px
W = DESIGN * S

def canvas():
    return Image.new("RGBA", (W, W), (0, 0, 0, 0))

def mix(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(4))

def vgrad(draw, box, top, bottom):
    x0, y0, x1, y1 = box
    for y in range(int(y0), int(y1)):
        t = (y - y0) / max(1, (y1 - y0))
        draw.rectangle([x0, y, x1, y + 1], fill=mix(top, bottom, t))

def scaled_box(x, y, w, h):
    return [x * S, y * S, (x + w) * S, (y + h) * S]

def finish(im, name, out_dir):
    im = im.resize((N, N), Image.LANCZOS)
    im.save(os.path.join(out_dir, name))

OUT = "/home/user/C-Larp/assets/textures"
os.makedirs(OUT, exist_ok=True)

# ---------------------------------------------------------------- poop
def poop():
    im = canvas(); d = ImageDraw.Draw(im)
    dark = (74, 42, 18, 255)
    # three stacked blobs with gradients
    blobs = [(40, 148, 176, 70, (169, 112, 60, 255), (122, 74, 35, 255)),
             (58, 104, 140, 66, (183, 126, 71, 255), (135, 83, 41, 255)),
             (80, 66, 96, 52, (197, 140, 82, 255), (146, 91, 46, 255))]
    for x, y, w, h, top, bot in blobs:
        layer = canvas(); ld = ImageDraw.Draw(layer)
        vgrad(ld, scaled_box(x, y, w, h), top, bot)
        mask = Image.new("L", (W, W), 0)
        ImageDraw.Draw(mask).ellipse(scaled_box(x, y, w, h), fill=255)
        im.paste(layer, (0, 0), mask)
        d.ellipse(scaled_box(x, y, w, h), outline=dark, width=5 * S)
    # swirl tip: a curl that grows out of the top blob, not a ball on a stick
    d.ellipse(scaled_box(112, 42, 54, 44), fill=(204, 148, 88, 255), outline=dark, width=4 * S)
    d.rectangle(scaled_box(114, 66, 50, 18), fill=(197, 140, 82, 255))
    d.arc(scaled_box(118, 46, 40, 30), 200, 340, fill=(120, 72, 34, 255), width=4 * S)
    # highlight
    d.ellipse(scaled_box(74, 124, 44, 24), fill=(226, 178, 120, 90))
    # eyes
    for ex in (96, 138):
        d.ellipse(scaled_box(ex, 126, 24, 26), fill=(255, 255, 255, 255), outline=dark, width=3 * S)
        d.ellipse(scaled_box(ex + 8, 134, 9, 11), fill=(38, 28, 22, 255))
        d.ellipse(scaled_box(ex + 10, 136, 4, 4), fill=(255, 255, 255, 255))
    # smile
    d.arc(scaled_box(100, 150, 60, 24), 20, 160, fill=dark, width=5 * S)
    return im

# ---------------------------------------------------------------- cake
def cake():
    im = canvas(); d = ImageDraw.Draw(im)
    dark = (92, 54, 36, 255)
    # plate
    d.ellipse(scaled_box(28, 186, 200, 40), fill=(230, 231, 245, 70))
    d.ellipse(scaled_box(36, 190, 184, 30), fill=(255, 255, 255, 110))
    # body
    d.rounded_rectangle(scaled_box(56, 104, 144, 96), radius=16 * S, fill=(246, 220, 176, 255), outline=dark, width=4 * S)
    # cream stripe
    d.rectangle(scaled_box(58, 140, 140, 18), fill=(236, 186, 132, 255))
    # frosting top
    d.rounded_rectangle(scaled_box(48, 88, 160, 44), radius=20 * S, fill=(244, 143, 177, 255), outline=dark, width=4 * S)
    for i, x in enumerate(range(56, 200, 26)):
        d.ellipse(scaled_box(x, 118, 30, 34), fill=(244, 143, 177, 255), outline=dark, width=3 * S)
    # cherry
    d.line([(128 * S, 82 * S), (140 * S, 58 * S)], fill=(80, 120, 52, 255), width=5 * S)
    d.ellipse(scaled_box(112, 52, 34, 34), fill=(226, 62, 78, 255), outline=(140, 30, 42, 255), width=3 * S)
    d.ellipse(scaled_box(118, 58, 12, 10), fill=(255, 178, 178, 220))
    # sprinkles
    for x, y, c in [(84, 96, (255, 235, 120, 255)), (150, 98, (120, 210, 255, 255)),
                    (108, 108, (170, 240, 170, 255)), (172, 112, (255, 190, 120, 255))]:
        d.rounded_rectangle(scaled_box(x, y, 14, 7), radius=3 * S, fill=c)
    # face
    for ex in (104, 138):
        d.ellipse(scaled_box(ex, 160, 14, 16), fill=(70, 44, 30, 255))
    d.arc(scaled_box(104, 172, 48, 20), 20, 160, fill=dark, width=4 * S)
    return im

# ---------------------------------------------------------------- bear
def bear():
    im = canvas(); d = ImageDraw.Draw(im)
    dark = (70, 44, 24, 255)
    fur, fur2 = (169, 113, 63, 255), (198, 140, 84, 255)
    # ears
    for ex in (44, 160):
        d.ellipse(scaled_box(ex, 40, 54, 54), fill=fur, outline=dark, width=4 * S)
        d.ellipse(scaled_box(ex + 12, 52, 30, 30), fill=(224, 172, 105, 255))
    # head
    layer = canvas(); ld = ImageDraw.Draw(layer)
    vgrad(ld, scaled_box(32, 62, 192, 172), fur2, fur)
    mask = Image.new("L", (W, W), 0)
    ImageDraw.Draw(mask).ellipse(scaled_box(32, 62, 192, 172), fill=255)
    im.paste(layer, (0, 0), mask)
    d.ellipse(scaled_box(32, 62, 192, 172), outline=dark, width=5 * S)
    # muzzle
    d.ellipse(scaled_box(84, 138, 88, 72), fill=(232, 196, 148, 255), outline=dark, width=4 * S)
    d.ellipse(scaled_box(110, 146, 36, 26), fill=(52, 36, 26, 255))
    d.arc(scaled_box(108, 166, 40, 26), 20, 160, fill=dark, width=4 * S)
    # eyes
    for ex in (84, 140):
        d.ellipse(scaled_box(ex, 110, 24, 26), fill=(255, 255, 255, 255), outline=dark, width=3 * S)
        d.ellipse(scaled_box(ex + 8, 118, 9, 11), fill=(40, 30, 24, 255))
        d.ellipse(scaled_box(ex + 10, 120, 4, 4), fill=(255, 255, 255, 255))
    # cheeks
    for ex in (58, 172):
        d.ellipse(scaled_box(ex, 138, 26, 18), fill=(236, 128, 128, 90))
    return im

# ---------------------------------------------------------------- rocket
def rocket_base():
    im = canvas(); d = ImageDraw.Draw(im)
    dark = (44, 54, 78, 255)
    # flame
    d.polygon([(100 * S, 206 * S), (128 * S, 244 * S), (156 * S, 206 * S)], fill=(255, 152, 40, 255))
    d.polygon([(112 * S, 206 * S), (128 * S, 232 * S), (144 * S, 206 * S)], fill=(255, 226, 92, 255))
    # fins
    d.polygon([(66 * S, 150 * S), (38 * S, 200 * S), (78 * S, 192 * S)], fill=(226, 72, 82, 255), outline=dark)
    d.polygon([(190 * S, 150 * S), (218 * S, 200 * S), (178 * S, 192 * S)], fill=(226, 72, 82, 255), outline=dark)
    # body
    layer = canvas(); ld = ImageDraw.Draw(layer)
    vgrad(ld, scaled_box(76, 56, 104, 152), (250, 250, 255, 255), (176, 186, 210, 255))
    mask = Image.new("L", (W, W), 0)
    ImageDraw.Draw(mask).rounded_rectangle(scaled_box(76, 56, 104, 152), radius=42 * S, fill=255)
    im.paste(layer, (0, 0), mask)
    d.rounded_rectangle(scaled_box(76, 56, 104, 152), radius=42 * S, outline=dark, width=4 * S)
    # nose cone
    d.polygon([(76 * S, 92 * S), (128 * S, 24 * S), (180 * S, 92 * S)], fill=(232, 76, 88, 255), outline=dark)
    # window
    d.ellipse(scaled_box(100, 96, 56, 56), fill=(84, 196, 240, 255), outline=dark, width=4 * S)
    d.ellipse(scaled_box(110, 104, 22, 20), fill=(210, 245, 255, 210))
    # stripe
    d.rectangle(scaled_box(78, 166, 100, 18), fill=(232, 76, 88, 255))
    return im

def rocket():
    base = rocket_base()
    return base.rotate(-14, resample=Image.BICUBIC, center=(W // 2, W // 2))

# ---------------------------------------------------------------- emerald
def emerald():
    """A white emerald: the colour comes from the hue layer in gift_icon()
    (gift_icons.inc), and it only shows on white - on a coloured sprite the hue
    turns into mud."""
    im = canvas(); d = ImageDraw.Draw(im)
    dark = (56, 56, 64, 255)

    def P(x, y):
        return (x * S, y * S)

    top, table_y = 28, 96
    L, R = 46, 210          # widest crown points
    bl, br = 72, 184        # bottom corners
    bot = 206
    # table (top facet), left lit, right shaded
    d.polygon([P(128, top), P(R, 88), P(128, table_y), P(L, 88)], fill=(252, 252, 255, 255))
    d.polygon([P(128, top), P(128, table_y), P(L, 88)], fill=(234, 234, 241, 255))
    d.polygon([P(128, top), P(R, 88), P(128, table_y)], fill=(210, 210, 220, 255))
    # pavilion halves
    d.polygon([P(128, table_y), P(L, 88), P(bl, bot), P(128, bot)], fill=(233, 233, 240, 255))
    d.polygon([P(128, table_y), P(R, 88), P(br, bot), P(128, bot)], fill=(198, 198, 210, 255))
    # bright centre facet
    d.polygon([P(128, table_y), P(162, 116), P(128, 170), P(94, 116)], fill=(255, 255, 255, 255))
    d.polygon([P(128, 100), P(150, 116), P(128, 154), P(106, 116)], fill=(243, 243, 249, 255))
    # seams
    d.line([P(128, table_y), P(L, 88), P(bl, bot), P(128, bot), P(br, bot), P(R, 88), P(128, table_y)],
           fill=(132, 132, 142, 255), width=4 * S)
    d.line([P(L, 88), P(128, table_y), P(R, 88)], fill=(132, 132, 142, 255), width=4 * S)
    d.line([P(128, 28), P(L, 88)], fill=(132, 132, 142, 255), width=4 * S)
    d.line([P(128, 28), P(R, 88)], fill=(132, 132, 142, 255), width=4 * S)
    d.line([P(128, table_y), P(bl, bot)], fill=(132, 132, 142, 255), width=4 * S)
    d.line([P(128, table_y), P(br, bot)], fill=(132, 132, 142, 255), width=4 * S)
    # outline
    d.line([P(128, 28), P(R, 88), P(br, bot), P(bl, bot), P(L, 88), P(128, 28)],
           fill=dark, width=6 * S, joint="curve")
    # sparkle
    d.line([P(86, 56), P(106, 56)], fill=(255, 255, 255, 255), width=5 * S)
    d.line([P(96, 46), P(96, 66)], fill=(255, 255, 255, 255), width=5 * S)
    return im

if __name__ == "__main__":
    finish(poop(), "gift_poop.png", OUT)
    finish(cake(), "gift_cake.png", OUT)
    finish(bear(), "gift_bear.png", OUT)
    finish(rocket(), "gift_rocket.png", OUT)
    finish(emerald(), "gift_emerald.png", OUT)
