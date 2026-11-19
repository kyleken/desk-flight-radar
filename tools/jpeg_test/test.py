import subprocess, os, io, random, numpy as np
from PIL import Image, ImageDraw, ImageFilter
import mozjpeg_lossless_optimization as mz

random.seed(1); np.random.seed(1)
SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'simulator', 'sample_photo.jpg')

def photo(w, h, seed):
    rng = np.random.default_rng(seed)
    base = Image.open(SRC).convert('RGB').resize((w, h), Image.LANCZOS) if seed % 2 == 0 else None
    if base is None:
        yy, xx = np.mgrid[0:h, 0:w]
        a = np.stack([(xx * 255 / max(w-1,1)), (yy * 255 / max(h-1,1)), ((xx + yy) % 64) * 4], -1)
        a = a + rng.normal(0, 18, a.shape)
        base = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))
        d = ImageDraw.Draw(base)
        for _ in range(8):
            x0, y0 = rng.integers(0, w), rng.integers(0, h)
            d.ellipse([x0, y0, x0 + rng.integers(5, 60), y0 + rng.integers(5, 40)],
                      fill=tuple(int(v) for v in rng.integers(0, 255, 3)))
        d.text((5, 5), "N8535S", fill=(255, 255, 255))
    return base

def run(data, w, h, maxwork=4000000):
    open('in.jpg', 'wb').write(data)
    r = subprocess.run(['./harness', 'in.jpg', str(w), str(h), 'out.raw', str(maxwork)], capture_output=True, text=True)
    if r.returncode != 0:
        return None, (r.stdout + r.stderr)[-400:]
    raw = np.frombuffer(open('out.raw', 'rb').read(), dtype='<u2').reshape(h, w).astype(np.int32)
    rgb = np.stack([((raw >> 11) & 31) * 255 // 31, ((raw >> 5) & 63) * 255 // 63, (raw & 31) * 255 // 31], -1)
    return rgb, r.stdout

def reference(img, w, h):
    # same cover crop, box filter on the full decode
    W, H = img.size
    s = max(w / W, h / H)
    if s > 1: return None
    sw, sh = W * s, H * s
    ox, oy = max(0.0, (sw - w) / 2 / s), max(0.0, (sh - h) / 2 / s)
    return np.asarray(img.convert('RGB').resize((w, h), Image.BOX, box=(ox, oy, ox + w / s, oy + h / s))).astype(np.int32)

def psnr(a, b):
    m = ((a - b) ** 2).mean()
    return 99 if m == 0 else 10 * np.log10(255 ** 2 / m)

fails = 0
low = 0
cases = 0
OUT = (88, 59)
for (w, h) in [(200, 136), (200, 133), (280, 187), (420, 280), (157, 101), (88, 59), (95, 70), (640, 427), (33, 21), (1, 1), (17, 300)]:
    for sub in [0, 1, 2]:     # 4:4:4, 4:2:2, 4:2:0
        for gray in [False, True]:
            if gray and sub: continue
            img = photo(w, h, w + h + sub)
            if gray: img = img.convert('L')
            for q in [60, 85, 95]:
                b = io.BytesIO(); img.save(b, 'JPEG', quality=q, subsampling=sub); base = b.getvalue()
                p = io.BytesIO(); img.save(p, 'JPEG', quality=q, subsampling=sub, progressive=True); prog = p.getvalue()
                moz = mz.optimize(base)  # mozjpeg progressive scan script
                variants = {'baseline': base, 'progressive': prog, 'mozjpeg': moz}
                # restart intervals via ImageMagick (libjpeg) progressive
                if q == 85:
                    open('tmp.jpg', 'wb').write(base)
                    subprocess.run(['convert', 'tmp.jpg', '-define', 'jpeg:restart-interval=3', '-interlace', 'Plane', '-quality', '85', 'rst.jpg'], check=True)
                    variants['magick-rst'] = open('rst.jpg', 'rb').read()
                    subprocess.run(['convert', 'tmp.jpg', '-define', 'jpeg:restart-interval=2', '-quality', '85', 'rstb.jpg'], check=True)
                    variants['magick-rst-baseline'] = open('rstb.jpg', 'rb').read()
                outs = {}
                for name, data in variants.items():
                    cases += 1
                    got, msg = run(data, *OUT)
                    if got is None:
                        fails += 1; print('FAIL', w, h, sub, gray, q, name, msg); continue
                    ref = reference(Image.open(io.BytesIO(data)), *OUT)
                    ps = 99 if ref is None else psnr(got, ref)
                    outs[name] = got
                    if ps < 30 and min(w, h) > 8:
                        low += 1; print('LOW', w, h, sub, gray, q, name, round(ps, 1))
                # same coefficients -> identical output
                for name in ('progressive', 'mozjpeg'):
                    if name in outs and 'baseline' in outs and not np.array_equal(outs[name], outs['baseline']):
                        fails += 1; d = np.abs(outs[name] - outs['baseline']).max()
                        print('DIFF', w, h, sub, gray, q, name, 'max', d)
print(f'{cases} decodes, {fails} failures ({low} LOW reference-score notes)')
raise SystemExit(1 if fails else 0)
