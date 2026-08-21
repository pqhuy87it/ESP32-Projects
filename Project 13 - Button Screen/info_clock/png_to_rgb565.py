#!/usr/bin/env python3
"""
png_to_rgb565.py — chuyen icon PNG thanh mang RGB565 cho TFT_eSPI::pushImage()

Cach dung:
    python3 png_to_rgb565.py --size 40 --out wx_icons.h icon_sunny.png icon_cloudy.png ...

Anh RGBA duoc ghep len nen den (vi giao dien cua ta nen den), nen vung trong
suot tu dong thanh den va khong can xu ly alpha tren MCU.
"""

import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("Can Pillow:  pip install pillow")


def to_rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def convert(path: str, size: int, bg=(0, 0, 0)):
    """Doc PNG, tra ve (rgb565[], alpha[]).

    RGB duoc ghep len nen den => day la dang ALPHA NHAN TRUOC
    (premultiplied). Nho vay khi ve len nen bat ky chi can:

        out = rgb + bg * (255 - alpha) / 255

    Thu tu hai buoc khong duoc doi cho nhau:
      1. Ghep len nen TRUOC
      2. Thu nho SAU
    Neu thu nho RGBA truoc, resize lay trung binh kenh RGB ma khong quan tam
    alpha; pixel trong suot (RGB thuong la den) se bi tron vao vien -> quang toi.

    Khi THU NHO dung BOX chu khong dung LANCZOS: LANCZOS gay ringing, tao vong
    toi quanh vien sang.
    """
    img = Image.open(path).convert("RGBA")

    alpha = img.getchannel("A")
    base  = Image.new("RGBA", img.size, tuple(bg) + (255,))
    flat  = Image.alpha_composite(base, img).convert("RGB")

    if flat.size != (size, size):
        resample = Image.BOX if size < flat.size[0] else Image.LANCZOS
        flat  = flat.resize((size, size), resample)
        alpha = alpha.resize((size, size), resample)

    fp, ap = flat.load(), alpha.load()
    rgb  = [to_rgb565(*fp[x, y]) for y in range(size) for x in range(size)]
    a    = [ap[x, y]             for y in range(size) for x in range(size)]
    return rgb, a


def ident(path: str) -> str:
    name = os.path.splitext(os.path.basename(path))[0]
    return "".join(c if c.isalnum() else "_" for c in name).upper()


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+")
    ap.add_argument("--size", type=int, default=40, help="canh vuong dau ra (px)")
    ap.add_argument("--out", default="weather_icons.h")
    ap.add_argument("--symbol-prefix", default="ICON_",
                    help="tien to ten mang, vd ICONSM_ cho bo icon nho")
    ap.add_argument("--size-name", default="WX_ICON_SIZE",
                    help="ten hang so kich thuoc, vd WX_ICON_SM_SIZE")
    ap.add_argument("--bg", default="0,0,0",
                    help="mau nen de ghep alpha, dang R,G,B (mac dinh den)")
    args = ap.parse_args()

    bg = tuple(int(v) for v in args.bg.split(","))

    lines = [
        "// Tu dong sinh boi png_to_rgb565.py — dung sua tay.",
        f"// Icon {args.size}x{args.size}, RGB565 (alpha nhan truoc) + kenh alpha rieng.",
        "// Nguon icon: https://github.com/MakerM0/MagiClick-watch",
        "#pragma once",
        "#include <stdint.h>",
        "",
        f"static constexpr int {args.size_name} = {args.size};",
        "",
    ]

    total = 0
    alpha_flag = args.symbol_prefix.rstrip("_") + "_HAS_ALPHA"
    lines.append(f"#define {alpha_flag} 1")
    lines.append("")

    for path in args.files:
        rgb, a = convert(path, args.size, bg)
        total += len(rgb) * 2 + len(a)
        name = args.symbol_prefix + ident(path).removeprefix("ICON_")

        lines.append(f"static const uint16_t {name}[{len(rgb)}] = {{")
        for i in range(0, len(rgb), 12):
            lines.append("  " + ", ".join(f"0x{v:04X}" for v in rgb[i:i + 12]) + ",")
        lines.append("};")
        lines.append("")

        lines.append(f"static const uint8_t {name}_A[{len(a)}] = {{")
        for i in range(0, len(a), 20):
            lines.append("  " + ", ".join(str(v) for v in a[i:i + 20]) + ",")
        lines.append("};")
        lines.append("")
        print(f"  {name:24} {len(rgb)*2:>6} B mau + {len(a):>5} B alpha")

    with open(args.out, "w") as fh:
        fh.write("\n".join(lines))

    print(f"\nDa ghi {args.out} — tong {total} byte flash "
          f"({total/1024:.1f} KB)")


if __name__ == "__main__":
    main()
