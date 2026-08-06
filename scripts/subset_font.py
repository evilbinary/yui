#!/usr/bin/env python3
"""
字体子集化工具：从大 TTF 中提取用到的字符，生成小字体（几 KB~几十 KB）。
用于 ESP32/STM32 嵌入式平台，配合 Flash 分区烧录。

用法：
  python3 scripts/subset_font.py --input app/assets/Roboto-Regular.ttf \
      --output build/font-subset.ttf --scan app/ --extra "你好世界设置连接"

依赖：pip install fonttools

字符来源（取并集）：
  1. --scan 目录下所有 .json 文件中 "text"/"label"/"title"/"message" 等字段的值
  2. --extra 指定的额外字符
  3. ASCII 可见字符 (0x20-0x7E) 默认包含
"""
import argparse
import json
import os
import sys

try:
    from fontTools import subset
except ImportError:
    print("error: need fonttools, run: pip install fonttools", file=sys.stderr)
    sys.exit(1)

# JSON 中需扫描的字段名（含 icon，emoji 图标也进子集）
TEXT_FIELDS = {
    "text", "label", "title", "message", "placeholder",
    "value", "name", "desc", "description", "hint", "tooltip",
    "button_text", "ok_text", "cancel_text", "icon",
}

# 常用中文字符（约 3500 常用字，这里列最常见的一小部分，可按需扩充）
COMMON_CJK = """的一是不了在人我有他这中大为来上国个到说们时要就出会以也你时能对生而子
里后去过得于着下自之年发回作好小多才这不外加用里后里向体供全关点正业外
将两制新设连其起本感问切此话什意象些主样理同现三法起所水十无二心相通然
前所日手高没天知重下各地进此门化少给位立女内使次明行东化当何度期公者住
难名世平思类型组工第成话候形步外按命件路图组选界转结类号常它总先光门连
因报效广及力史打保部南算世身真务视电张做件期群元林况照带任影级验收改风
极即院马候半失布办青议色设准流据达火近量步太际石列状城系场转米书报土直
设置连接删除新建编辑保存取消确定搜索输入用户密码名称地址端口数据库查询
表数据行列刷新打开关闭最小化最大化偏好关于退出复制剪切粘贴撤销重做全选
首页上下返回菜单工具帮助主题语言中文英文加载错误警告信息提示成功失败"""


def scan_json_files(scan_dir):
    """扫描目录下所有 JSON 文件的文本字段，收集字符。"""
    chars = set()
    if not scan_dir or not os.path.isdir(scan_dir):
        return chars
    for root, _dirs, files in os.walk(scan_dir):
        for fname in files:
            if not fname.endswith(".json"):
                continue
            fpath = os.path.join(root, fname)
            try:
                with open(fpath, "r", encoding="utf-8") as f:
                    data = json.load(f)
            except Exception:
                continue
            _collect_text(data, chars)
    return chars


def scan_js_files(scan_dir):
    """扫描目录下所有 .js 源码，收集其中出现的非 ASCII 字符（emoji 图标等）。"""
    chars = set()
    if not scan_dir or not os.path.isdir(scan_dir):
        return chars
    for root, _dirs, files in os.walk(scan_dir):
        for fname in files:
            if not fname.endswith(".js"):
                continue
            fpath = os.path.join(root, fname)
            try:
                with open(fpath, "r", encoding="utf-8") as f:
                    data = f.read()
            except Exception:
                continue
            chars.update(ch for ch in data if ord(ch) > 0x7F)
    return chars


def _collect_text(obj, chars):
    """递归收集 JSON 中的文本字段值。"""
    if isinstance(obj, dict):
        for k, v in obj.items():
            if k in TEXT_FIELDS and isinstance(v, str):
                chars.update(v)
            else:
                _collect_text(v, chars)
    elif isinstance(obj, list):
        for item in obj:
            _collect_text(item, chars)


def _merge_emoji_font(font, emoji_path, text):
    """把 emoji 字体中主字体缺失的字形复制进主字体（字符级合并）。

    处理要点：
    - 高码位(>0xFFFF)必须进 cmap format12，不能写进 format4（会溢出）。
    - TTFont.getBestCmap() 返回 subtable.cmap 的引用，必须拷贝后再改，
      否则会把高码位写进 format4。
    """
    import copy
    from fontTools.ttLib import TTFont
    from fontTools.ttLib.tables._c_m_a_p import CmapSubtable

    emoji_font = TTFont(emoji_path)
    emoji_opts = subset.Options()
    emoji_opts.glyph_names = False
    emoji_opts.notdef_outline = True
    emoji_font = subset.load_font(emoji_path, emoji_opts)
    sub = subset.Subsetter(options=emoji_opts)
    sub.populate(text=text)
    sub.subset(emoji_font)

    main_cmap = dict(font.getBestCmap())
    emoji_cmap = emoji_font.getBestCmap()
    main_glyf = font["glyf"]
    emoji_glyf = emoji_font["glyf"]
    main_hmtx = font["hmtx"]
    emoji_hmtx = emoji_font["hmtx"]

    added = 0
    high = {}
    newnames = []
    # 主字体与 emoji 字体的 upm 通常不同（本工程 256 vs 2048）。若不缩放，
    # emoji 字形(advance ~2600)在主字体 256-upm 下会放大 ~8-10 倍 → 图标巨大/溢出。
    scale = float(font["head"].unitsPerEm) / float(emoji_font["head"].unitsPerEm)
    tmat = (scale, 0, 0, scale, 0, 0)
    for cp, sname in emoji_cmap.items():
        if cp in main_cmap:
            continue
        newname = "em%x" % cp
        if newname in main_glyf:
            continue
        g = copy.deepcopy(emoji_glyf[sname])
        # fontTools 5.x 的 glyf 项是 Glyph 包装对象：简单字形缩放 coordinates，
        # 复合字形缩放各 component 的变换/偏移。
        coords = getattr(g, "coordinates", None)
        if coords is not None:
            coords.transform([[scale, 0], [0, scale]])
        comps = getattr(g, "components", None)
        if comps:
            for comp in comps:
                ct = getattr(comp, "transform", None)
                if ct:
                    comp.transform = (ct[0] * scale, ct[1] * scale,
                                      ct[2] * scale, ct[3] * scale,
                                      ct[4] * scale, ct[5] * scale)
        main_glyf[newname] = g
        eadv, elsb = emoji_hmtx[sname]
        main_hmtx[newname] = (int(round(eadv * scale)), int(round(elsb * scale)))
        if "vmtx" in font and "vmtx" in emoji_font:
            vadv, vtop = emoji_font["vmtx"][sname]
            font["vmtx"][newname] = (int(round(vadv * scale)), int(round(vtop * scale)))
        elif "vmtx" in font:
            font["vmtx"][newname] = (0, 0)
        if cp <= 0xFFFF:
            for table in font["cmap"].tables:
                if table.isUnicode():
                    table.cmap[cp] = newname
        else:
            high[cp] = newname
        main_cmap[cp] = newname
        newnames.append(newname)
        added += 1

    if newnames:
        # FontGlyphs 的 __setitem__ 已自动维护 glyf 的 glyphOrder（与 ttFont.glyphOrder
        # 同引用），这里只需把尚未注册的新名字补进全局 order，避免重复。
        order = font.getGlyphOrder()
        font.glyphOrder = order + [n for n in newnames if n not in order]
        font["maxp"].numGlyphs = len(font.glyphOrder)

    if high and not any(t.format == 12 for t in font["cmap"].tables):
        t12 = CmapSubtable.newSubtable(12)
        t12.platformID = 3
        t12.platEncID = 10
        t12.language = 0
        t12.cmap = dict(high)
        font["cmap"].tables.append(t12)
        font["cmap"].tableVersion = 0

    print(f"emoji merge: added {added} glyphs ({len(high)} high codepoints) from {emoji_path}")


def main():
    parser = argparse.ArgumentParser(description="YUI 字体子集化工具")
    parser.add_argument("--input", required=True, help="输入 TTF 路径")
    parser.add_argument("--output", required=True, help="输出子集 TTF 路径")
    parser.add_argument("--scan", default="", help="扫描目录（提取 JSON 文本字段）")
    parser.add_argument("--extra", default="", help="额外字符")
    parser.add_argument("--no-cjk", action="store_true", help="不包含常用中文字符")
    parser.add_argument("--emoji-input", default="", help="附加 emoji/符号 TTF，子集化后合并进输出（补主字体缺失字形）")
    args = parser.parse_args()

    chars = set()
    # ASCII 可见字符
    chars.update(chr(c) for c in range(0x20, 0x7F))
    # 常用中文
    if not args.no_cjk:
        chars.update(COMMON_CJK)
    # 扫描 JSON
    if args.scan:
        scanned = scan_json_files(args.scan)
        print(f"scanned {len(scanned)} unique chars from {args.scan}")
        chars.update(scanned)
        js_scanned = scan_js_files(args.scan)
        print(f"scanned {len(js_scanned)} non-ascii chars from .js files")
        chars.update(js_scanned)
    # 额外字符
    if args.extra:
        chars.update(args.extra)

    text = "".join(sorted(chars))
    print(f"total unique chars: {len(chars)}")

    options = subset.Options()
    options.glyph_names = False
    options.name_IDs = ["*"]
    options.notdef_outline = True
    options.recalc_bounds = True
    options.drop_tables = ["DSIG", "MVAR", "cvar", "kern"]

    font = subset.load_font(args.input, options)
    subsetter = subset.Subsetter(options=options)
    subsetter.populate(text=text)
    subsetter.subset(font)

    # 合并 emoji 字体缺失字形（如 NotoEmoji 的高码位符号/emoji）
    if args.emoji_input:
        try:
            from fontTools.ttLib.tables._c_m_a_p import CmapSubtable
        except ImportError:
            CmapSubtable = None
        if CmapSubtable is None:
            print("warning: fontTools cmap module unavailable, skip emoji merge", file=sys.stderr)
        else:
            _merge_emoji_font(font, args.emoji_input, text)

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    font.save(args.output)
    in_size = os.path.getsize(args.input)
    out_size = os.path.getsize(args.output)
    print(f"input:  {in_size:>10} bytes")
    print(f"output: {out_size:>10} bytes  ({100.0 * out_size / in_size:.1f}%)")


if __name__ == "__main__":
    main()
