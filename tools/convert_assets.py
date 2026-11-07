#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Converts the original Little Fighter (1995) data into GBA-ready binaries.
#
# Usage: convert_assets.py <Little_Fighter_1995.zip> <output directory>
#
# The original game is 320x200 with 256-colour sprites. The GBA screen is 240x160,
# so every picture is squeezed to 3/4 of its width (the battlefield keeps its full
# 144-pixel height), and the game world is laid out in 240-pixel units to match.

import os
import struct
import sys
import zipfile


SQUEEZE_NUM, SQUEEZE_DEN = 3, 4
STAGE_HEIGHT = 144
STAGE_COUNT = 5
FIGHTER_SLOTS = 8
TINT_BASE = 151          # original palette indices that hold per-fighter colours
TINT_COUNT = 10          # nine clothing colours plus hair
DEFAULT_VARIANT = FIGHTER_SLOTS
TEAM_COLORS = [1, 16, 31, 46, 65, 108, 126, 144]   # Round::assign_colors in the logic
HP_COLOR = 31            # original health bar colour
ENERGY_COLOR = 106       # original energy bar colour
DARK_COLOR = 2           # empty part of a bar
MARKER_GLYPHS = "0123456789c"
BAR_FILLS = 11           # 0..10 pixels of the 10-pixel bar
TINT_SETS = 12           # colour sets: one per fighter slot (0-7) plus four more for menu icons
ICON_ROWS = 14           # rows of the idle body shown under the head in an icon (clear of the HUD strip's bottom line)

# Menu and HUD colours, as the nearest palette entries to these 6-bit RGB targets.
PANEL_RGB = (4, 8, 24)
FRAME_RGB = (26, 44, 63)
HILITE_RGB = (46, 30, 4)
HUD_RGB = (3, 6, 18)
HUD_LINE_RGB = (14, 26, 52)
HUD_EDGE_RGB = (30, 48, 63)
UI_HILITE_BASE = 96      # menu tile numbers: panel font 0-95, highlighted font 96-191,
UI_FRAME_BASE = 192      # eight frame tiles, then the backdrop picture
UI_TILES = 200
LOGO_ROWS = (22, 96)     # rows of MAIN.GRH holding the logo
PAL_RGB = []             # filled in by main(): index -> 6-bit RGB


def read_grh(archive, name):
    data = archive.read("LitFight/SYS/" + name)
    width = data[0] * 256 + data[1]
    height = data[2] * 256 + data[3]
    pixels = data[300:]
    if width * height != len(pixels):
        raise SystemExit(f"{name}: bad header")
    return width, height, pixels


def cell(sheet, width, x, y, w, h):
    return [sheet[(y + r) * width + x + c] for r in range(h) for c in range(w)]


def squeeze(pixels, w, h, new_w):
    """Nearest-neighbour horizontal resample; new_w = round(w * 3 / 4)."""
    cols = [min(w - 1, (d * 2 + 1) * w // (2 * new_w)) for d in range(new_w)]
    return [pixels[r * w + cols[d]] for r in range(h) for d in range(new_w)], new_w, h


def squeezed_cell(pixels, width, x, y, w, h):
    new_w = (w * SQUEEZE_NUM + SQUEEZE_DEN // 2) // SQUEEZE_DEN
    out, _, _ = squeeze(cell(pixels, width, x, y, w, h), w, h, new_w)
    return out, new_w, h


def place(pixels, w, h, cw, ch):
    """Pads a w x h picture into a cw x ch cell with transparency (0) around it."""
    out = [0] * (cw * ch)
    for r in range(h):
        for c in range(w):
            out[r * cw + c] = pixels[r * w + c]
    return out


def to_tiles(cell_pixels, cw, ch):
    """8bpp tiles, row-major within the sprite, each tile 64 bytes row-major."""
    out = bytearray()
    for ty in range(ch // 8):
        for tx in range(cw // 8):
            for r in range(8):
                row = (ty * 8 + r) * cw + tx * 8
                out += bytes(cell_pixels[row:row + 8])
    return bytes(out)


def nearest(target):
    best, best_d = 1, 1 << 30
    for v in range(1, 256):
        d = sum((a - b) ** 2 for a, b in zip(PAL_RGB[v], target))
        if d < best_d:
            best, best_d = v, d
    return best


def bgr15(r6, g6, b6):
    def five(v):
        return (v * 255 // 63) >> 3
    return five(r6) | (five(g6) << 5) | (five(b6) << 10)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    convert(sys.argv[1], sys.argv[2])


def convert(zip_path, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    archive = zipfile.ZipFile(zip_path)

    raw_pal = archive.read("LitFight/SYS/PAL")
    if len(raw_pal) != 768:
        raise SystemExit("PAL must hold 256 colours")
    # Hardware index v holds palette file entry v-1, index 0 is black (as the original).
    rgb = [(0, 0, 0)] + [tuple(raw_pal[i * 3:i * 3 + 3]) for i in range(255)]
    colors = [bgr15(*c) for c in rgb]
    PAL_RGB[:] = rgb

    act1 = read_grh(archive, "ACT1.GRH")
    act2 = read_grh(archive, "ACT2.GRH")
    heads = read_grh(archive, "HEAD.GRH")
    items = read_grh(archive, "WEAPON.GRH")
    font = read_grh(archive, "NEWFONTS")

    # ---- Fighter bodies: 60 poses, 8 colour variants plus the fixed-colour default.
    def body_cell(sheet, pose):
        x, y = (pose % 12) * 26 + 1, (pose // 12) * 26 + 1
        pixels, w, h = squeezed_cell(sheet[2], sheet[0], x, y, 25, 25)
        return pixels, w, h

    def tint_variant(pixels, variant, tint_index):
        if variant == DEFAULT_VARIANT:
            return pixels
        return [tint_index[variant][v - TINT_BASE] if TINT_BASE <= v < TINT_BASE + TINT_COUNT else v for v in pixels]

    poses = 60
    body_raw = [body_cell(act1, p) for p in range(poses)]
    extra_raw = [body_cell(act2, p) for p in range(poses)]
    shadow_pixels, shadow_w, shadow_h = squeezed_cell(act1[2], act1[0], 1, 127, 25, 3)

    heads_raw = []
    for head in range(55):
        x, y = (head % 11) * 18 + 1, (head // 11) * 13 + 1
        pixels, w, h = squeezed_cell(heads[2], heads[0], x, y, 17, 12)
        heads_raw.append(place(pixels, w, h, 16, 16))

    items_raw = []
    for image in range(120):
        x, y = (image % 15) * 18 + 1, (image // 15) * 14 + 1
        pixels, w, h = squeezed_cell(items[2], items[0], x, y, 17, 13)
        items_raw.append(place(pixels, w, h, 16, 16))

    blasts_raw = []
    for frame in range(3):
        pixels, w, h = squeezed_cell(items[2], items[0], frame * 36 + 127, 81, 35, 29)
        blasts_raw.append(place(pixels, w, h, 32, 32))

    # ---- Markers: font glyphs, coloured as the original does (glyph value + tint).
    glyph_index = {ch: ord(ch) for ch in MARKER_GLYPHS}
    marker_tints = [tc - 15 for tc in TEAM_COLORS] + [0]
    markers_raw = []   # [variant][glyph] -> 8x8 cell

    def glyph_pixels(code):
        x, y = (code % 16) * 6, (code // 16) * 8
        return cell(font[2], font[0], x, y, 5, 7)

    for tint in marker_tints:
        row = []
        for ch in MARKER_GLYPHS:
            g = glyph_pixels(glyph_index[ch])
            g = [((v + tint) & 0xFF) if v else 0 for v in g]
            row.append(place(g, 5, 7, 8, 8))
        markers_raw.append(row)

    # ---- Bars: 16x8 sprites (two 8x8 tiles), a 4-pixel band, filled 0..12 pixels.
    bars_raw = []
    for color in (HP_COLOR, ENERGY_COLOR):
        row = []
        for fill in range(BAR_FILLS):
            cell_px = [0] * (16 * 8)
            for y in range(4):
                for x in range(10):
                    cell_px[y * 16 + x] = color if x < fill else DARK_COLOR
            row.append(cell_px)
        bars_raw.append(row)

    # ---- Palette: every OBJ colour used by any sprite keeps its original colour.
    used = set()
    for variant in [body_raw[p][0] for p in range(poses)]:
        used.update(variant)
    for p in extra_raw:
        used.update(p[0])
    used.update(shadow_pixels)
    for p in heads_raw + items_raw + [b for b in blasts_raw]:
        used.update(p)
    for row in markers_raw:
        for g in row:
            used.update(g)
    for row in bars_raw:
        for p in row:
            used.update(p)
    used.discard(0)
    free = [v for v in range(1, 256) if v not in used]
    if len(free) < TINT_SETS * TINT_COUNT:
        raise SystemExit("Not enough free sprite palette entries for fighter colours")
    tint_index = [free[n * TINT_COUNT:(n + 1) * TINT_COUNT] for n in range(TINT_SETS)]

    pal_obj = [0] * 256
    for v in used:
        pal_obj[v] = colors[v]

    # ---- Write sprite data.
    def body_bytes(variant):
        out = bytearray()
        for pixels, w, h in body_raw:
            out += to_tiles(place(tint_variant(pixels, variant, tint_index), w, h, 32, 32), 32, 32)
        return out

    body = bytearray()
    for variant in range(FIGHTER_SLOTS):
        body += body_bytes(variant)
    body += body_bytes(DEFAULT_VARIANT)
    for pixels, w, h in extra_raw:
        body += to_tiles(place(pixels, w, h, 32, 32), 32, 32)

    write(out_dir, "body.bin", body)
    write(out_dir, "shadow.bin", to_tiles(place(shadow_pixels, shadow_w, shadow_h, 32, 8), 32, 8))
    write(out_dir, "heads.bin", b"".join(to_tiles(p, 16, 16) for p in heads_raw))
    write(out_dir, "items.bin", b"".join(to_tiles(p, 16, 16) for p in items_raw))
    write(out_dir, "blasts.bin", b"".join(to_tiles(p, 32, 32) for p in blasts_raw))
    write(out_dir, "markers.bin", b"".join(to_tiles(g, 8, 8) for row in markers_raw for g in row))
    write(out_dir, "bars.bin", b"".join(to_tiles(p, 16, 8) for row in bars_raw for p in row))

    # ---- Icons: the head over the top of the idle body, in each colour set, as 32x16 cells.
    body_pixels, body_w, body_h = body_raw[0]
    icons = bytearray()
    for variant in range(TINT_SETS):
        tinted = [tint_index[variant][v - TINT_BASE] if TINT_BASE <= v < TINT_BASE + TINT_COUNT else v for v in body_pixels]
        for head in range(55):
            x, y = (head % 11) * 18 + 1, (head // 11) * 13 + 1
            head_pixels, head_w, head_h = squeezed_cell(heads[2], heads[0], x, y, 17, 12)
            cell_px = [0] * (32 * 16)
            for r in range(ICON_ROWS):
                for c in range(body_w):
                    cell_px[r * 32 + c] = tinted[r * body_w + c]
            for r in range(head_h):
                for c in range(head_w):
                    if head_pixels[r * head_w + c]:
                        cell_px[r * 32 + c] = head_pixels[r * head_w + c]
            icons += to_tiles(cell_px, 32, 16)
    write(out_dir, "icons.bin", bytes(icons))

    font_tiles = bytearray()
    for code in range(32, 128):
        font_tiles += to_tiles(place(glyph_pixels(code), 5, 7, 8, 8), 8, 8)
    write(out_dir, "font.bin", font_tiles)

    # ---- Stages: 240 pixels per picture half, 480 wide, 144 high.
    stage_sizes = []
    stage_images = []
    for s in range(STAGE_COUNT):
        left = read_grh(archive, f"BACK{s + 2}1.GRH")
        right = read_grh(archive, f"BACK{s + 2}2.GRH")
        width, height = 480, STAGE_HEIGHT
        image = [0] * (width * height)
        for half, (w, h, pixels) in enumerate((left, right)):
            squeezed, new_w, _ = squeeze(pixels, w, h, w * SQUEEZE_NUM // SQUEEZE_DEN)
            for r in range(height):
                image[r * width + half * new_w:r * width + half * new_w + new_w] = squeezed[r * new_w:(r + 1) * new_w]
        stage_images.append(image)
        tiles, tile_map = build_stage_tiles(image, width, height)
        write(out_dir, f"stage{s}.bin", tiles)
        stage_sizes.append(len(tiles) // 64)
        write(out_dir, f"stage{s}_map.bin", tile_map)

    write(out_dir, "pal_bg.bin", b"".join(struct.pack("<H", c) for c in colors))
    write(out_dir, "pal_obj.bin", b"".join(struct.pack("<H", c) for c in pal_obj))
    write(out_dir, "data.bin", archive.read("LitFight/SYS/DATA.DAT"))

    write_header(out_dir, tint_index, stage_sizes)
    write_sounds(archive, out_dir)
    write_menus(archive, out_dir, stage_images[1], font)
    write_banner_and_moves(archive, out_dir, font)
    write_incbin(out_dir)
    print(f"assets written to {out_dir}: stage tile counts {stage_sizes}")


def build_stage_tiles(image, width, height):
    """Unique 8x8 tiles with flips, and a 64x32 map in two 32x32 screen blocks.
    Tile 0 is blank; map rows 0-1 are the battle HUD strip, stage rows start at 2."""
    blank = bytes(64)
    tiles = [blank]
    index = {blank: 0}
    tile_map = [[0] * 64 for _ in range(32)]
    for ty in range(height // 8):
        for tx in range(width // 8):
            pixels = bytes(image[(ty * 8 + r) * width + tx * 8 + c] for r in range(8) for c in range(8))
            flips = {
                (0, 0): pixels,
                (1, 0): bytes(pixels[r * 8 + 7 - c] for r in range(8) for c in range(8)),
                (0, 1): bytes(pixels[(7 - r) * 8 + c] for r in range(8) for c in range(8)),
                (1, 1): bytes(pixels[(7 - r) * 8 + 7 - c] for r in range(8) for c in range(8)),
            }
            found = None
            for flip, data in flips.items():
                if data in index:
                    found = (index[data], flip)
                    break
            if found is None:
                index[pixels] = len(tiles)
                tiles.append(pixels)
                found = (index[pixels], (0, 0))
            number, (hflip, vflip) = found
            if number > 959:
                raise SystemExit("stage needs more than 959 tiles")
            tile_map[ty + 2][tx] = number | (hflip << 10) | (vflip << 11)
    # HUD strip: a dark panel with a light line along its lower edge.
    fill, line, edge = nearest(HUD_RGB), nearest(HUD_LINE_RGB), nearest(HUD_EDGE_RGB)
    hud_top = bytes([fill] * 64)
    hud_bottom = bytes([fill] * 48 + [line] * 8 + [edge] * 8)
    for tile, row in ((hud_top, 0), (hud_bottom, 1)):
        index[tile] = len(tiles)
        tiles.append(tile)
        for tx in range(64):
            tile_map[row][tx] = index[tile]
    # Screen block layout: block 0 holds columns 0-31, block 1 columns 32-63.
    blocks = bytearray()
    for block in range(2):
        for row in range(32):
            for col in range(32):
                blocks += struct.pack("<H", tile_map[row][block * 32 + col])
    return b"".join(tiles), bytes(blocks)


# Sound table order: battle sounds SA..SR (BattleSound index 1..18 is entry 0..17),
# then the menu sounds: change (B3), confirm (B2), toggle (B1).
SOUND_PADDING = 512
SOUND_FILES = ["SA", "SB", "SC", "SD", "SE", "SF", "SG", "SH", "SI", "SJ", "SK", "SL", "SM", "SN", "SO", "SP", "SQ", "SR",
               "B3", "B2", "B1"]


def read_wav(archive, name):
    """Returns (sample rate, unsigned 8-bit mono samples) of a RIFF WAV file."""
    data = archive.read("LitFight/SYS/" + name + ".WAV")
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise SystemExit(f"{name}: not a WAV file")
    pos, rate, bits, channels, samples = 12, None, None, None, None
    while pos + 8 <= len(data):
        chunk = data[pos:pos + 4]
        size = struct.unpack("<I", data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if chunk == b"fmt ":
            _, channels, rate, _, _, bits = struct.unpack("<HHIIHH", body[:16])
        elif chunk == b"data":
            samples = body
        pos += 8 + size + (size & 1)
    if samples is None or bits != 8 or channels != 1:
        raise SystemExit(f"{name}: expected 8-bit mono PCM")
    return rate, samples


def write_sounds(archive, out_dir):
    """Signed 8-bit samples for the GBA's DMA sound FIFO, each padded to a 4-byte boundary."""
    blob = bytearray()
    entries = []
    for name in SOUND_FILES:
        rate, samples = read_wav(archive, name)
        offset = len(blob)
        # Unsigned 8-bit to signed: subtract 128, which is the same as flipping the top bit.
        blob += bytes(b ^ 0x80 for b in samples)
        blob += bytes(SOUND_PADDING)       # silence after the end, in case playback runs a little long
        blob += bytes((4 - len(blob) % 4) % 4)
        entries.append((offset, len(samples), rate))
    write(out_dir, "sounds.bin", bytes(blob))
    lines = ["static const uint32_t SOUND_OFFSET[SOUND_COUNT] = {" + ", ".join(str(e[0]) for e in entries) + "};",
             "static const uint32_t SOUND_LENGTH[SOUND_COUNT] = {" + ", ".join(str(e[1]) for e in entries) + "};",
             "static const uint32_t SOUND_RATE[SOUND_COUNT] = {" + ", ".join(str(e[2]) for e in entries) + "};",
             "#define SOUND_MENU_CHANGE 18", "#define SOUND_MENU_CONFIRM 19", "#define SOUND_MENU_TOGGLE 20", ""]
    with open(os.path.join(out_dir, "assets.h"), "a") as f:
        f.write("#define SOUND_COUNT %d\n" % len(entries))
        f.write("\n".join(lines) + "\n")


def pack_map(tile_map):
    """A 64x32 map as two 32x32 screen blocks."""
    blocks = bytearray()
    for block in range(2):
        for row in range(32):
            for col in range(32):
                blocks += struct.pack("<H", tile_map[row][block * 32 + col])
    return bytes(blocks)


def add_image(tiles, index, image, width, height, tile_map, first_row=0):
    """Appends the unique tiles (with flips) of a picture and fills the map rows for it."""
    for ty in range(height // 8):
        for tx in range(width // 8):
            pixels = bytes(image[(ty * 8 + r) * width + tx * 8 + c] for r in range(8) for c in range(8))
            flips = {
                (0, 0): pixels,
                (1, 0): bytes(pixels[r * 8 + 7 - c] for r in range(8) for c in range(8)),
                (0, 1): bytes(pixels[(7 - r) * 8 + c] for r in range(8) for c in range(8)),
                (1, 1): bytes(pixels[(7 - r) * 8 + 7 - c] for r in range(8) for c in range(8)),
            }
            found = None
            for flip, data in flips.items():
                if data in index:
                    found = (index[data], flip)
                    break
            if found is None:
                index[pixels] = len(tiles)
                tiles.append(pixels)
                found = (index[pixels], (0, 0))
            number, (hflip, vflip) = found
            if number > 959:
                raise SystemExit("picture needs more than 959 tiles")
            tile_map[first_row + ty][tx] = number | (hflip << 10) | (vflip << 11)


def ui_tiles(font):
    """Panel text, highlighted text and panel frame tiles, shared by every menu screen."""
    panel, hilite, frame = nearest(PANEL_RGB), nearest(HILITE_RGB), nearest(FRAME_RGB)

    def glyph(code):
        x, y = (code % 16) * 6, (code // 16) * 8
        return place(cell(font[2], font[0], x, y, 5, 7), 5, 7, 8, 8)

    tiles = []
    for background in (panel, hilite):
        for code in range(32, 128):
            tiles.append(bytes(v if v else background for v in glyph(code)))

    def frame_tile(top, bottom, left, right):
        return bytes(frame if (top and y == 0) or (bottom and y == 7) or (left and x == 0) or (right and x == 7) else panel
                     for y in range(8) for x in range(8))

    tiles += [frame_tile(1, 0, 1, 0), frame_tile(1, 0, 0, 0), frame_tile(1, 0, 0, 1),
              frame_tile(0, 0, 1, 0), frame_tile(0, 0, 0, 1),
              frame_tile(0, 1, 1, 0), frame_tile(0, 1, 0, 0), frame_tile(0, 1, 0, 1)]
    if len(tiles) != UI_TILES:
        raise SystemExit("unexpected UI tile count")
    return tiles


def write_menus(archive, out_dir, stage_image, font):
    """Menu and title screens: the UI tiles followed by a night-road picture (and the logo for the title)."""
    width, height = 240, 160
    # The strip above the 144-row battlefield is filled with the sky's most common colour.
    sky = max(set(stage_image[0:240]), key=stage_image[0:240].count)
    backdrop = [sky] * (240 * 16) + [v for r in range(144) for v in stage_image[r * 480:r * 480 + 240]]

    logo_w, logo_h, logo = read_grh(archive, "MAIN.GRH")
    top, bottom = LOGO_ROWS
    rows = [logo[r * logo_w + 21:r * logo_w + 304] for r in range(top, bottom)]
    flat = [v for row in rows for v in row]
    squeezed, sq_w, sq_h = squeeze(flat, 283, bottom - top, 212)
    title = list(backdrop)
    ox, oy = (width - sq_w) // 2, 10
    for r in range(sq_h):
        for c in range(sq_w):
            v = squeezed[r * sq_w + c]
            if v:
                title[(oy + r) * width + ox + c] = v

    counts = []
    for name, image in (("menu", backdrop), ("title", title)):
        tiles = ui_tiles(font)
        tile_map = [[0] * 64 for _ in range(32)]
        add_image(tiles, {}, image, width, height, tile_map)
        write(out_dir, name + ".bin", b"".join(tiles))
        write(out_dir, name + "_map.bin", pack_map(tile_map))
        counts.append(len(tiles))
    with open(os.path.join(out_dir, "assets.h"), "a") as f:
        f.write(f"#define MENU_TILES {counts[0]}\n#define TITLE_TILES {counts[1]}\n")
        f.write(f"#define UI_HILITE_BASE {UI_HILITE_BASE}\n#define UI_FRAME_BASE {UI_FRAME_BASE}\n")
        f.write(f"#define UI_PANEL_COLOR {nearest(PANEL_RGB)}\n")


BANNER_GLYPHS = "FIGHT!K.OPAUSED"   # "FIGHT!" at the start of a round, "K.O." at its end, "PAUSED"
MOVES_ROWS = (56, 152)        # rows of a SUPER?.GRH portrait that list the special moves
MOVES_TILES = (10, 12)        # the moves card is 10x12 tiles after squeezing 106 columns to 80


def write_banner_and_moves(archive, out_dir, font):
    """Banner letters as 16x16 sprite cells (the letter in the middle, to be scaled up), and the
    special-moves card of each of the eleven fighters as 10x12 background tiles."""
    cells = bytearray()
    for ch in BANNER_GLYPHS:
        code = ord(ch)
        glyph = cell(font[2], font[0], (code % 16) * 6, (code // 16) * 8, 5, 7)
        pixels = [0] * (16 * 16)
        for r in range(7):
            for c in range(5):
                pixels[(4 + r) * 16 + 5 + c] = glyph[r * 5 + c]
        cells += to_tiles(pixels, 16, 16)
    write(out_dir, "banner.bin", bytes(cells))

    cols, rows = MOVES_TILES
    moves = bytearray()
    for letter in "ABCDEFGHIJK":
        w, h, pixels = read_grh(archive, f"SUPER{letter}.GRH")
        top, bottom = MOVES_ROWS
        region = pixels[top * w:bottom * w]
        squeezed, sq_w, sq_h = squeeze(region, w, bottom - top, cols * 8)
        for ty in range(rows):
            for tx in range(cols):
                moves += bytes(squeezed[(ty * 8 + r) * sq_w + tx * 8 + c] for r in range(8) for c in range(8))
    write(out_dir, "moves.bin", bytes(moves))
    with open(os.path.join(out_dir, "assets.h"), "a") as f:
        f.write(f"#define BANNER_GLYPHS {len(BANNER_GLYPHS)}\n#define MOVES_COLUMNS {cols}\n#define MOVES_ROWS {rows}\n")


def write_incbin(out_dir):
    """Assembly that embeds every .bin file in ROM as a symbol named <file>_bin."""
    lines = [".section .rodata", ".balign 4"]
    for name in sorted(os.listdir(out_dir)):
        if not name.endswith(".bin"):
            continue
        symbol = name.replace(".", "_")
        lines += [f".global {symbol}", f"{symbol}:", f'    .incbin "{name}"', f".global {symbol}_end", f"{symbol}_end:", ".balign 4"]
    write(out_dir, "assets.S", ("\n".join(lines) + "\n").encode())


def write(out_dir, name, data):
    with open(os.path.join(out_dir, name), "wb") as f:
        f.write(data)


def write_header(out_dir, tint_index, stage_sizes):
    lines = [
        "/* Generated by tools/convert_assets.py. Do not edit. */",
        "#pragma once",
        "#include <stdint.h>",
        "",
        f"#define FIGHTER_SLOTS {FIGHTER_SLOTS}",
        f"#define TINT_SETS {TINT_SETS}",
        f"#define TINT_COUNT {TINT_COUNT}",
        f"#define DEFAULT_VARIANT {DEFAULT_VARIANT}",
        f"#define STAGE_COUNT {STAGE_COUNT}",
        f"#define STAGE_HEIGHT {STAGE_HEIGHT}",
        f"#define BODY_POSES 60",
        f"#define HEAD_COUNT 55",
        f"#define ITEM_COUNT 120",
        f"#define BLAST_FRAMES 3",
        f"#define MARKER_GLYPHS {len(MARKER_GLYPHS)}",
        f"#define MARKER_VARIANTS {len(TEAM_COLORS) + 1}",
        f"#define BAR_FILLS {BAR_FILLS}",
        "",
        "/* Sprite palette indices of each fighter slot's ten colours. */",
        "static const uint8_t TINT_INDEX[TINT_SETS][TINT_COUNT] = {",
    ]
    for row in tint_index:
        lines.append("    {" + ", ".join(str(v) for v in row) + "},")
    lines += [
        "};",
        "",
        "static const uint16_t STAGE_TILES[STAGE_COUNT] = {" + ", ".join(str(v) for v in stage_sizes) + "};",
        "",
        "static const uint8_t TEAM_COLORS[8] = {" + ", ".join(str(v) for v in TEAM_COLORS) + "};",
        "",
    ]
    write(out_dir, "assets.h", ("\n".join(lines) + "\n").encode())


if __name__ == "__main__":
    main()
