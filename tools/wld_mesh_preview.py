#!/usr/bin/env python3
"""Render an EQ .wld 0x36 mesh (by name) to a PNG so we can SEE a model without a
game client. Parses the classic DmSpriteDef geometry (i16 verts + scale, triangle
list), projects it from a few angles, flat-shades with a painter's algorithm, and
writes a PPM -> PNG via ImageMagick. Read-only; used to verify what a model looks
like (e.g. classic IT7 mace vs. an in-game render).

Usage: wld_mesh_preview.py <archive.s3d> <MESHNAME e.g. IT7_DMSPRITEDEF> [--out out.png]
"""
import os, sys, struct, zlib, math, subprocess

XOR = bytes([0x95, 0x3A, 0xC5, 0x2A, 0x95, 0x7A, 0x95, 0x6A])

def _inflate(d, off, size):
    out = bytearray(); p = off
    while len(out) < size:
        dl, il = struct.unpack_from('<II', d, p); p += 8
        out += zlib.decompress(bytes(d[p:p + dl])); p += dl
    return bytes(out)

def extract_wld(path):
    d = open(path, 'rb').read()
    diroff = struct.unpack_from('<I', d, 0)[0]; off = diroff
    n = struct.unpack_from('<I', d, off)[0]; off += 4
    ent = [struct.unpack_from('<III', d, off + i * 12) for i in range(n)]
    off += n * 12
    names = []
    for crc, fo, sz in ent:
        if crc == 0x61580AC9:
            raw = _inflate(d, fo, sz); p = 0; fc = struct.unpack_from('<I', raw, p)[0]; p += 4
            for _ in range(fc):
                ln = struct.unpack_from('<I', raw, p)[0]; p += 4
                names.append(raw[p:p + ln - 1].decode('latin-1')); p += ln
            break
    de = sorted([e for e in ent if e[0] != 0x61580AC9], key=lambda e: e[1])
    wname = next(nm for nm, e in zip(names, de) if nm.lower().endswith('.wld'))
    widx = names.index(wname)
    return _inflate(d, de[widx][1], de[widx][2])

def wld_strings(wld):
    hashlen = struct.unpack_from('<IIIIIII', wld, 0)[5]
    dec = bytes(b ^ XOR[i & 7] for i, b in enumerate(wld[28:28 + hashlen]))
    # map offset -> string
    m = {}
    i = 0
    for part in dec.split(b'\x00'):
        m[i] = part.decode('latin-1', 'replace')
        i += len(part) + 1
    return dec, m

def frag_iter(wld):
    """Yield (index1based, type, nameref, body_off, body_len)."""
    hashlen = struct.unpack_from('<IIIIIII', wld, 0)[5]
    fragcount = struct.unpack_from('<IIIIIII', wld, 0)[2]
    p = 28 + hashlen
    for i in range(fragcount):
        size, typ = struct.unpack_from('<II', wld, p)
        body = p + 8
        nameref = struct.unpack_from('<i', wld, body)[0]
        yield i + 1, typ, nameref, body, size  # size = bytes after the 8-byte (size,type) header
        p = body + size

def find_mesh(wld, meshname):
    dec, off2str = wld_strings(wld)
    for idx, typ, nameref, body, blen in frag_iter(wld):
        if typ == 0x36 and nameref < 0:
            nm = off2str.get(-nameref, '')
            if nm.upper() == meshname.upper():
                return wld, body, blen
    return None, None, None

def parse_mesh_36(wld, body):
    # header offsets per EQEmu 0x36 reference
    cx, cy, cz = struct.unpack_from('<fff', wld, body + 0x18)
    vcount, tcount, ncount, ccount, pcount = struct.unpack_from('<HHHHH', wld, body + 0x4C)
    scale = struct.unpack_from('<H', wld, body + 0x5E)[0]
    p = body + 0x60
    inv = 1.0 / (1 << scale)
    verts = []
    for _ in range(vcount):
        x, y, z = struct.unpack_from('<hhh', wld, p); p += 6
        verts.append((cx + x * inv, cy + y * inv, cz + z * inv))
    p += tcount * 4          # tex coords (old fmt i16,i16)
    p += ncount * 3          # normals i8
    p += ccount * 4          # colors u32
    faces = []
    for _ in range(pcount):
        flags, a, b, c = struct.unpack_from('<HHHH', wld, p); p += 8
        faces.append((a, b, c))
    return verts, faces, (cx, cy, cz), (vcount, pcount, scale)

def render(verts, faces, out, w=360, h=360):
    if not verts:
        print("no verts"); return
    xs = [v[0] for v in verts]; ys = [v[1] for v in verts]; zs = [v[2] for v in verts]
    cx = (min(xs) + max(xs)) / 2; cy = (min(ys) + max(ys)) / 2; cz = (min(zs) + max(zs)) / 2
    span = max(max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs)) or 1.0
    views = [(0.6, 0.5), (0.6, 1.9), (0.2, 3.1)]  # (pitch, yaw) radians
    panels = []
    for pitch, yaw in views:
        buf = [[(28, 30, 40)] * w for _ in range(h)]
        zbuf = [[-1e9] * w for _ in range(h)]
        cosp, sinp = math.cos(pitch), math.sin(pitch)
        cosy, siny = math.cos(yaw), math.sin(yaw)
        def project(v):
            x, y, z = v[0] - cx, v[1] - cy, v[2] - cz
            x, z = x * cosy - z * siny, x * siny + z * cosy      # yaw
            y, z = y * cosp - z * sinp, y * sinp + z * cosp      # pitch
            sc = (w * 0.7) / span
            return (w / 2 + x * sc, h / 2 - y * sc, z)
        pv = [project(v) for v in verts]
        light = (0.3, 0.5, 0.8)
        order = sorted(range(len(faces)), key=lambda f: (pv[faces[f][0]][2] + pv[faces[f][1]][2] + pv[faces[f][2]][2]))
        for fi in order:
            a, b, c = faces[fi]
            if max(a, b, c) >= len(pv):
                continue
            (ax, ay, az), (bx, by, bz), (cxp, cyp, cz2) = pv[a], pv[b], pv[c]
            # normal in view space for shading
            ux, uy, uz = bx - ax, by - ay, bz - az
            vx, vy, vz = cxp - ax, cyp - ay, cz2 - az
            nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
            nl = math.sqrt(nx * nx + ny * ny + nz * nz) or 1
            sh = abs((nx * light[0] + ny * light[1] + nz * light[2]) / nl)
            col = (int(70 + 150 * sh), int(70 + 140 * sh), int(80 + 120 * sh))
            minx, maxx = int(max(0, min(ax, bx, cxp))), int(min(w - 1, max(ax, bx, cxp)))
            miny, maxy = int(max(0, min(ay, by, cyp))), int(min(h - 1, max(ay, by, cyp)))
            denom = ((by - cyp) * (ax - cxp) + (cxp - bx) * (ay - cyp)) or 1e-9
            for py in range(miny, maxy + 1):
                for px in range(minx, maxx + 1):
                    l1 = ((by - cyp) * (px - cxp) + (cxp - bx) * (py - cyp)) / denom
                    l2 = ((cyp - ay) * (px - cxp) + (ax - cxp) * (py - cyp)) / denom
                    l3 = 1 - l1 - l2
                    if l1 >= -0.01 and l2 >= -0.01 and l3 >= -0.01:
                        zz = l1 * az + l2 * bz + l3 * cz2
                        if zz > zbuf[py][px]:
                            zbuf[py][px] = zz; buf[py][px] = col
        panels.append(buf)
    # stitch panels horizontally
    full_w = w * len(panels)
    ppm = f"P6\n{full_w} {h}\n255\n".encode()
    rows = bytearray()
    for y in range(h):
        for buf in panels:
            for x in range(w):
                rows += bytes(buf[y][x])
    open(out + '.ppm', 'wb').write(ppm + bytes(rows))
    subprocess.run(['magick', out + '.ppm', out], check=False)
    os.remove(out + '.ppm')
    print(f"rendered {out} ({full_w}x{h})")

def main():
    arc, mesh = sys.argv[1], sys.argv[2]
    out = sys.argv[sys.argv.index('--out') + 1] if '--out' in sys.argv else '/tmp/mesh.png'
    wld = extract_wld(arc)
    w, body, blen = find_mesh(wld, mesh)
    if body is None:
        print(f"mesh {mesh} not found"); return
    verts, faces, center, stats = parse_mesh_36(wld, body)
    print(f"{mesh}: {stats[0]} verts, {stats[1]} polys, scale={stats[2]}, center={tuple(round(c,1) for c in center)}")
    render(verts, faces, out)

if __name__ == '__main__':
    main()
