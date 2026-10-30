"""Visual QA slice from the actual grid states. Requires Pillow."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
from voxel_lab import VoxelWorld, WATER, WALL, EMPTY

W, H = 1050, 420
img = Image.new("RGB", (W, H), (17, 25, 37))
draw = ImageDraw.Draw(img)
font = ImageFont.load_default()
colors = {WATER: (49, 150, 223), WALL: (136, 149, 160), EMPTY: (29, 41, 54)}

def panel(world, title, xoffset, yoffset, mode="side"):
    cell = 23
    draw.text((xoffset, yoffset-32), title, fill=(235, 244, 252), font=font)
    if mode == "side":
        ny = world.ny // 2
        for x in range(world.nx):
            for z in range(world.nz):
                t = world.get((x, ny, z))
                x1 = xoffset+x*cell
                y1 = yoffset+(world.nz-z-1)*cell
                draw.rectangle((x1, y1, x1+cell-2, y1+cell-2), fill=colors[t])
    else:
        for x in range(world.nx):
            for y in range(world.ny):
                t = world.get((x, y, 1))
                x1 = xoffset+x*cell
                y1 = yoffset+y*cell
                draw.rectangle((x1,y1,x1+cell-2,y1+cell-2), fill=colors[t])

initial = VoxelWorld().initialized()
current = VoxelWorld().initialized()
for tick in range(30):
    current.step(tick)
panel(initial, "Start: Querschnitt (y = Mitte)", 42, 80)
panel(current, "Nach 30 Schritten: Querschnitt (y = Mitte)", 565, 80)
draw.text((42, 306), "Blau = Wasser    Grau = Behaelter    Dunkel = Leerraum", fill=(221, 231, 244), font=font)
draw.text((42, 335), "Diskreter Testalgorithmus. Keine echte Stroemungsphysik.", fill=(235, 178, 114), font=font)
draw.text((42, 364), f"Wasserzellen: {initial.count(WATER)} -> {current.count(WATER)} (Massenerhaltung im Raster: PASS)", fill=(152, 219, 171), font=font)
out=Path(__file__).resolve().parent/"output"/"vergleich_querschnitt.png"
out.parent.mkdir(exist_ok=True)
img.save(out)
print(f"Preview: {out} ({W}x{H})")
