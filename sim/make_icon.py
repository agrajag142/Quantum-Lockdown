# Puts sce_sys/icon0.png together (512x512) from icon_layers.cpp's layers: a night sky over a burning city, a
# mushroom cloud, a missile rising, the quantum core's blue orbits, the agent running (lit blue), a catwalk, the
# title in glowing vector letters (QUANTUM cyan, LOCKDOWN red).   python3 make_icon.py <layers dir> <out.png>
import sys, math, random
from PIL import Image, ImageDraw, ImageFilter, ImageChops
D = sys.argv[1]; out = sys.argv[2]
S = 1024
random.seed(7)
img = Image.new('RGB', (S, S))
dr = ImageDraw.Draw(img)
# sky: deep navy at the top to a hot red-orange glow low down (the strikes on the horizon)
for y in range(S):
    k = y / S
    if k < 0.55:
        t = k / 0.55; c = (int(6 + 20 * t), int(8 + 10 * t), int(22 + 30 * t))
    else:
        t = (k - 0.55) / 0.45; c = (int(26 + 150 * t), int(18 + 40 * t), int(52 - 30 * t))
    dr.line([(0, y), (S, y)], fill=c)
for i in range(160):
    x, y = random.randrange(S), random.randrange(int(S * 0.5))
    b = random.randint(120, 255); r = random.choice([1, 1, 1, 2])
    dr.ellipse([x - r, y - r, x + r, y + r], fill=(b, b, min(255, b + 20)))
# far city skyline in silhouette, with a mushroom cloud glowing behind it on the right
glow = Image.new('RGB', (S, S)); gd = ImageDraw.Draw(glow)
gd.ellipse([620, 380, 1080, 840], fill=(255, 120, 40))
glow = glow.filter(ImageFilter.GaussianBlur(90))
img = ImageChops.add(img, glow)
dr = ImageDraw.Draw(img)
mx = 840
dr.rectangle([mx - 34, 560, mx + 34, 760], fill=(120, 50, 30))                       # its stem
dr.ellipse([mx - 150, 430, mx + 150, 610], fill=(150, 60, 34))                         # its cap
dr.ellipse([mx - 120, 410, mx + 110, 560], fill=(200, 96, 44))
dr.ellipse([mx - 70, 430, mx + 60, 520], fill=(255, 170, 80))
dr.ellipse([mx - 180, 690, mx + 180, 780], fill=(130, 54, 34))                         # the ring at its foot
random.seed(3)
x = 0
while x < S:                                                                           # the skyline
    w = random.randint(30, 80); h = random.randint(40, 170)
    dr.rectangle([x, 780 - h, x + w, 820], fill=(14, 10, 20))
    for wy in range(780 - h + 12, 800, 18):
        for wx in range(x + 8, x + w - 8, 14):
            if random.random() < 0.25: dr.rectangle([wx, wy, wx + 5, wy + 7], fill=(255, 190, 90))
    x += w + random.randint(0, 6)
# the quantum core: blue orbits round a glowing centre, behind the agent
core = Image.new('RGB', (S, S)); cd = ImageDraw.Draw(core)
cx, cy = 330, 470
cd.ellipse([cx - 90, cy - 90, cx + 90, cy + 90], fill=(40, 140, 255))
core = core.filter(ImageFilter.GaussianBlur(60))
img = ImageChops.add(img, core)
orb = Image.new('RGB', (S, S)); od = ImageDraw.Draw(orb)
for a in (0, 60, 120):
    pts = []
    for k in range(121):
        t = k / 120 * 2 * math.pi
        ex, ey = math.cos(t) * 250, math.sin(t) * 78
        ra = math.radians(a)
        pts.append((cx + ex * math.cos(ra) - ey * math.sin(ra), cy + ex * math.sin(ra) + ey * math.cos(ra)))
    od.line(pts, fill=(90, 200, 255), width=6)
    t = math.radians(a * 2 + 40)
    ex, ey = math.cos(t) * 250, math.sin(t) * 78
    ra = math.radians(a)
    px, py = cx + ex * math.cos(ra) - ey * math.sin(ra), cy + ex * math.sin(ra) + ey * math.cos(ra)
    od.ellipse([px - 14, py - 14, px + 14, py + 14], fill=(220, 250, 255))
orbGlow = orb.filter(ImageFilter.GaussianBlur(10))
img = ImageChops.add(img, orbGlow); img = ImageChops.add(img, orb)
dr = ImageDraw.Draw(img)
dr.ellipse([cx - 34, cy - 34, cx + 34, cy + 34], fill=(230, 250, 255))
# the missile: rising at a slant from the right, its flame and smoke trail
ang = math.radians(-62)                                                                # (up and to the left)
mxp, myp, L, W = 720, 300, 330, 46
ux, uy = math.cos(ang), math.sin(ang); vx, vy = -uy, ux
def P(a, b): return (mxp + ux * a + vx * b, myp + uy * a + vy * b)
trail = Image.new('RGB', (S, S)); td = ImageDraw.Draw(trail)
for k in range(14):
    a = -L / 2 - 40 - k * 46; r = 26 + k * 7
    x0, y0 = P(a, 0)
    v = max(30, 170 - k * 10)
    td.ellipse([x0 - r, y0 - r, x0 + r, y0 + r], fill=(v, v - 10, v - 20))
trail = trail.filter(ImageFilter.GaussianBlur(12))
img = ImageChops.screen(img, trail)
fl = Image.new('RGB', (S, S)); fd = ImageDraw.Draw(fl)
fd.polygon([P(-L / 2, -W * 0.45), P(-L / 2 - 170, 0), P(-L / 2, W * 0.45)], fill=(255, 120, 30))
fd.polygon([P(-L / 2, -W * 0.3), P(-L / 2 - 110, 0), P(-L / 2, W * 0.3)], fill=(255, 220, 90))
fd.polygon([P(-L / 2, -W * 0.15), P(-L / 2 - 60, 0), P(-L / 2, W * 0.15)], fill=(255, 255, 230))
img = ImageChops.add(img, fl.filter(ImageFilter.GaussianBlur(14))); img = ImageChops.add(img, fl)
dr = ImageDraw.Draw(img)
dr.polygon([P(-L / 2, -W / 2), P(L / 2 - 60, -W / 2), P(L / 2 - 60, W / 2), P(-L / 2, W / 2)], fill=(232, 234, 240))
dr.polygon([P(-L / 2, 0), P(L / 2 - 60, 0), P(L / 2 - 60, W / 2), P(-L / 2, W / 2)], fill=(186, 190, 200))
dr.polygon([P(L / 2 - 60, -W / 2), P(L / 2 + 10, 0), P(L / 2 - 60, W / 2)], fill=(210, 40, 34))           # the nose
for a in (-L / 2 + 70, 10):
    dr.polygon([P(a, -W / 2), P(a + 14, -W / 2), P(a + 14, W / 2), P(a, W / 2)], fill=(30, 32, 38))   # its bands
for s in (-1, 1):
    dr.polygon([P(-L / 2, s * W / 2), P(-L / 2 + 70, s * W / 2), P(-L / 2 - 6, s * (W / 2 + 46))], fill=(60, 64, 76))   # fins
# the agent, running (the game's own drawing), lit blue from behind
ag = Image.open(f'{D}/agent.ppm').convert('RGB')
bbox = ag.crop((0, 150, 900, 1080)).getbbox()
a = ag.crop((0, 150, 900, 1080)).crop(bbox)
sc = 560 / a.height
a = a.resize((int(a.width * sc), 560), Image.LANCZOS)
mask = a.convert('L').point(lambda v: 255 if v > 6 else 0)
ax, ay = 110, 806 - 560
rim = Image.new('RGB', (S, S)); rm = Image.new('L', (S, S)); rm.paste(mask, (ax, ay))
rimBlur = rm.filter(ImageFilter.GaussianBlur(16))
rim.paste((70, 180, 255), (0, 0, S, S), rimBlur)
img = ImageChops.add(img, rim)
img.paste(a, (ax, ay), mask)
# the ground: a catwalk with a hazard band, then the title on a dark band
dr = ImageDraw.Draw(img)
dr.rectangle([0, 800, S, 822], fill=(80, 86, 98)); dr.rectangle([0, 800, S, 805], fill=(160, 166, 178))
for x in range(-40, S, 48):
    dr.polygon([(x, 852), (x + 24, 852), (x + 54, 822), (x + 30, 822)], fill=(240, 196, 40))
dr.rectangle([0, 822, S, 852], outline=None)
band = Image.new('RGB', (S, 180), (6, 8, 14))
img.paste(band, (0, 852))
# the title: QUANTUM in cyan, LOCKDOWN in red, glowing (the game's own vector letters)
tt = Image.open(f'{D}/title.ppm').convert('L').filter(ImageFilter.MaxFilter(7))
q = tt.crop((0, 100, 1920, 250)); l = tt.crop((0, 375, 1920, 530))
def word(m, h):
    m = m.crop(m.getbbox()); w = int(m.width * h / m.height)
    return m.resize((w, h), Image.LANCZOS)
q = word(q, 100); l = word(l, 100)
scale = min(1.0, 940 / max(q.width, l.width))
if scale < 1: q = q.resize((int(q.width * scale), int(100 * scale)), Image.LANCZOS); l = l.resize((int(l.width * scale), int(100 * scale)), Image.LANCZOS)
def put(m, y, col, glowc):
    x = (S - m.width) // 2
    gl = Image.new('L', (S, S)); gl.paste(m, (x, y))
    g = Image.new('RGB', (S, S)); g.paste(glowc, (0, 0, S, S), gl.filter(ImageFilter.GaussianBlur(10)))
    global img
    img = ImageChops.add(img, g)
    solid = Image.new('RGB', m.size, col)
    img.paste(solid, (x, y), m)
put(q, 34, (150, 230, 255), (40, 140, 255))
put(l, 878, (255, 110, 80), (255, 50, 30))
img.resize((512, 512), Image.LANCZOS).save(out)
