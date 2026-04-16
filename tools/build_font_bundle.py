#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
IdolyPride Korean Font AssetBundle Builder
==========================================
Unity 2022.3.21f1 Android AssetBundle (Font ClassID=128) 생성.

Requirements: pip install UnityPy
Usage:
  python build_font_bundle.py                         # OTF 자동 탐색
  python build_font_bundle.py font.otf out.bundle     # 직접 지정
  python build_font_bundle.py font.otf out.bundle template.bundle  # 템플릿 사용
"""

import sys, os, io, struct, subprocess
from pathlib import Path

GAME_PKG  = "game.qualiarts.idolypride"
UNITY_VER = (2022, 3, 21, 1)   # (major, minor, patch, build)
FONT_CID  = 128                 # UnityEngine.Font ClassID
FONT_NAME = "KoreanFont"

# ─── 의존성 ──────────────────────────────────────────────────────────────────
def pip_install(pkg):
    try:
        __import__(pkg)
    except ImportError:
        print(f"  pip install {pkg} ...")
        subprocess.check_call([sys.executable, "-m", "pip", "install", pkg],
                              stdout=subprocess.DEVNULL)

pip_install("UnityPy")

import UnityPy
from UnityPy.helpers import Tpk
from UnityPy.helpers.TypeTreeHelper import write_typetree
from UnityPy.streams.EndianBinaryWriter import EndianBinaryWriter

# ─── 경로 설정 ───────────────────────────────────────────────────────────────
script_dir = Path(__file__).resolve().parent
repo_root  = script_dir.parent

OTF_CANDIDATES = [
    repo_root / "app/src/main/assets/hoshimi-local/local-files/gkamsZHFontMIX.otf",
    repo_root / "app/build/intermediates/assets/debug/mergeDebugAssets/hoshimi-local/local-files/gkamsZHFontMIX.otf",
    script_dir / "gkamsZHFontMIX.otf",
]

# argv 처리
otf_arg = Path(sys.argv[1]) if len(sys.argv) > 1 else None
out_arg  = Path(sys.argv[2]) if len(sys.argv) > 2 else None
tmpl_arg = Path(sys.argv[3]) if len(sys.argv) > 3 else None

if otf_arg and otf_arg.exists():
    otf_path = otf_arg
else:
    otf_path = next((p for p in OTF_CANDIDATES if p.exists()), None)

if not otf_path:
    print("[ERROR] OTF file not found.")
    print("  Usage: python build_font_bundle.py <font.otf> [output.bundle]")
    sys.exit(1)

out_bundle = out_arg if out_arg else (script_dir / "korean_font.bundle")
otf_data   = otf_path.read_bytes()

print(f"Font : {otf_path}  ({len(otf_data):,} bytes)")
print(f"Out  : {out_bundle}")
print()

# ─── Font dict 구성 ──────────────────────────────────────────────────────────
def make_font_dict():
    return {
        "m_Name":                       FONT_NAME,
        "m_LineSpacing":                1.0,
        "m_DefaultMaterial":            {"m_FileID": 0, "m_PathID": 0},
        "m_FontSize":                   0.0,
        "m_Texture":                    {"m_FileID": 0, "m_PathID": 0},
        "m_AsciiStartOffset":           0,
        "m_Tracking":                   0.0,
        "m_CharacterSpacing":           0,
        "m_CharacterPadding":           0,
        "m_ConvertCase":                0,
        "m_CharacterRects":             [],
        "m_KerningValues":              [],
        "m_PixelScale":                 0.1,
        "m_FontData":                   list(otf_data),
        "m_Ascent":                     0.0,
        "m_Descent":                    0.0,
        "m_DefaultStyle":               0,
        "m_FontNames":                  [FONT_NAME],
        "m_FallbackFonts":              [],
        "m_FontRenderingMode":          0,
        "m_UseLegacyBoundsCalculation": False,
        "m_ShouldRoundAdvanceValue":    True,
    }

# ─── Font 직렬화 ─────────────────────────────────────────────────────────────
def serialize_font() -> bytes:
    nodes = Tpk.get_typetree_nodes(FONT_CID, UNITY_VER)
    writer = EndianBinaryWriter(endian="<")
    write_typetree(make_font_dict(), nodes, writer)
    return bytes(writer.bytes)

# ─── SerializedFile v22 (Unity 2022.3.x) ────────────────────────────────────
def build_typetree_blob(nodes) -> bytes:
    """TypeTreeNode 리스트를 SerializedFile 형식으로 직렬화."""
    # string pool
    str_pool = bytearray()
    str_off  = {}
    def intern(s: str) -> int:
        if s not in str_off:
            str_off[s] = len(str_pool)
            str_pool.extend(s.encode() + b"\x00")
        return str_off[s]

    node_buf = bytearray()
    for n in nodes:
        t  = n.m_Type  or ""
        nm = n.m_Name  or ""
        ti = intern(t)
        ni = intern(nm)
        # Each node: version(u16) depth(u8) isArray(u8) typeOff(u32) nameOff(u32)
        #            byteSize(i32) index(u32) metaFlag(u32)  → 24 bytes total
        is_arr = 1 if (n.m_MetaFlag & 0x4000) else 0
        node_buf += struct.pack("<HBBIIiII",
            n.m_Version, n.m_Level, is_arr,
            ti, ni,
            n.m_ByteSize, n.m_Index, n.m_MetaFlag)

    sp = bytes(str_pool)
    hdr = struct.pack("<II", len(nodes), len(sp))
    return hdr + bytes(node_buf) + sp

def build_serialized_file(object_bytes: bytes) -> bytes:
    """Unity 2022.3.x SerializedFile version=22 생성."""
    TARGET_PLATFORM = 13   # BuildTarget.Android

    nodes = Tpk.get_typetree_nodes(FONT_CID, UNITY_VER)
    tt_blob = build_typetree_blob(nodes)

    # ── metadata 구성 ──
    md = bytearray()
    md += f"{'.'.join(str(v) for v in UNITY_VER[:3])}f{UNITY_VER[3]}\x00".encode()  # "2022.3.21f1\0"
    md += struct.pack("<I", TARGET_PLATFORM)       # target platform
    md += struct.pack("<B", 1)                     # has_type_tree = true

    # Type array: 1 entry (Font)
    md += struct.pack("<i", 1)                     # type count
    md += struct.pack("<i", FONT_CID)              # class_id
    md += struct.pack("<B", 0)                     # is_stripped
    md += struct.pack("<h", -1)                    # script_type_index
    md += bytes(32)                                # old_type_hash + type_hash (zeroed)
    md += tt_blob

    # Object table: 1 entry
    md += struct.pack("<I", 1)                     # object count
    # align to 4
    while len(md) % 4:
        md += b"\x00"
    md += struct.pack("<q", 1)                     # path_id
    md += struct.pack("<I", 0)                     # byte_start (relative to data section)
    md += struct.pack("<I", len(object_bytes))     # byte_size
    md += struct.pack("<I", 0)                     # type_id (index into type array)

    md += struct.pack("<I", 0)                     # script refs count
    md += struct.pack("<I", 0)                     # externals count
    md += struct.pack("<I", 0)                     # ref types count (v21+)
    md += b"\x00"                                  # user info (empty string)

    md_bytes = bytes(md)

    # ── header (v22: all little-endian) ──
    # fixed header layout: metadata_size(i32) file_size(i64) version(i32) data_offset(i64) endian(u8) pad(3)
    FIXED_HDR_SIZE = 4 + 8 + 4 + 8 + 1 + 3
    raw_data_offset = FIXED_HDR_SIZE + len(md_bytes)
    data_offset = ((raw_data_offset + 15) // 16) * 16   # align to 16
    file_size   = data_offset + len(object_bytes)

    hdr = struct.pack("<iqiq", len(md_bytes), file_size, 22, data_offset)
    hdr += struct.pack("<B", 0) + b"\x00\x00\x00"       # endian=LE + padding

    result = hdr + md_bytes
    result = result.ljust(data_offset, b"\x00")
    result += object_bytes
    return result

# ─── UnityFS 컨테이너 ─────────────────────────────────────────────────────────
def build_unityfs(sf_bytes: bytes) -> bytes:
    """UnityFS (version=9) 컨테이너 생성 (비압축)."""
    MAGIC   = b"UnityFS\x00"
    VER     = 9
    WEB_VER = b"5.x.x\x00"
    UNI_VER = f"{'.'.join(str(v) for v in UNITY_VER[:3])}f{UNITY_VER[3]}\x00".encode()
    CAB     = b"CAB-koreafont\x00"

    # Block info
    bi = struct.pack(">16s", bytes(16))                              # GUID (zeroed)
    bi += struct.pack(">I",   1)                                     # block count = 1
    bi += struct.pack(">II",  len(sf_bytes), len(sf_bytes))          # uncomp, comp size
    bi += struct.pack(">H",   0)                                     # flags = 0 (no compress)
    bi += struct.pack(">I",   1)                                     # dir entry count = 1
    bi += struct.pack(">qq",  0, len(sf_bytes))                      # offset=0, size
    bi += struct.pack(">I",   4)                                     # flags = 4 (SerializedFile)
    bi += CAB
    bi_bytes = bytes(bi)

    FLAGS = 0x40   # HasDirectoryInfo (block info NOT at end)

    # Header (sizes big-endian)
    hdr = MAGIC
    hdr += struct.pack(">I", VER)
    hdr += WEB_VER
    hdr += UNI_VER
    file_size = len(hdr) + 8 + 4 + 4 + 4 + len(bi_bytes) + len(sf_bytes)
    # Note: file_size field is at fixed offset after the strings, compute exact:
    # We'll patch it after
    hdr += b"\x00" * 8                                              # file_size placeholder
    hdr += struct.pack(">II", len(bi_bytes), len(bi_bytes))          # comp / uncomp block info
    hdr += struct.pack(">I",  FLAGS)
    hdr = bytearray(hdr)

    total = len(hdr) + len(bi_bytes) + len(sf_bytes)
    # Patch file_size: it's at offset = len(MAGIC)+4+len(WEB_VER)+len(UNI_VER)
    fs_offset = len(MAGIC) + 4 + len(WEB_VER) + len(UNI_VER)
    struct.pack_into(">q", hdr, fs_offset, total)

    return bytes(hdr) + bi_bytes + sf_bytes

# ─── ADB 헬퍼 ────────────────────────────────────────────────────────────────
def adb_pull_template() -> str | None:
    try:
        r = subprocess.run(["adb", "devices"], capture_output=True, text=True, timeout=5)
        if "device" not in r.stdout:
            print("  ADB device not connected. Skipping.")
            return None
    except Exception:
        print("  ADB not available. Skipping.")
        return None

    search = (
        "find /sdcard/Android/data/" + GAME_PKG +
        r" \( -name '*.bundle' -o -name '*.unity3d' \) -size -8M 2>/dev/null | head -3"
    )
    r2 = subprocess.run(["adb", "shell", search], capture_output=True, text=True, timeout=30)
    paths = [l.strip() for l in r2.stdout.splitlines() if l.strip()]
    if not paths:
        print("  No bundles found via ADB.")
        return None

    for remote in paths:
        local = str(script_dir / "_tmpl.bundle")
        r3 = subprocess.run(["adb", "pull", remote, local], capture_output=True, timeout=60)
        if r3.returncode == 0 and Path(local).exists():
            print(f"  Pulled: {remote} ({Path(local).stat().st_size:,} bytes)")
            return local
    return None

# ─── 템플릿 주입 ─────────────────────────────────────────────────────────────
def inject_into_template(tmpl: str) -> bool:
    print(f"[UnityPy] Loading: {tmpl}")
    try:
        env = UnityPy.load(tmpl)
    except Exception as e:
        print(f"  Load failed: {e}")
        return False

    for obj in env.objects:
        if obj.type.name == "Font":
            try:
                tree = obj.read_typetree()
                tree["m_FontData"] = list(otf_data)
                tree["m_Name"]     = FONT_NAME
                obj.save_typetree(tree)
                print(f"  Font injected ({len(otf_data):,} bytes)")
            except Exception as e:
                print(f"  Inject error: {e}")
                return False
            try:
                out_bundle.write_bytes(env.file.save())
                print(f"  Saved: {out_bundle} ({out_bundle.stat().st_size:,} bytes)")
                return True
            except Exception as e:
                print(f"  Save failed: {e}")
                return False

    print(f"  No Font object found. Types: {[o.type.name for o in env.objects]}")
    return False

# ─── 직접 생성 ───────────────────────────────────────────────────────────────
def create_from_scratch() -> bool:
    print("[Build] Serializing Font with TypeTree...")
    try:
        font_bytes = serialize_font()
        print(f"  Font bytes: {len(font_bytes):,}")
    except Exception as e:
        print(f"  serialize_font failed: {e}")
        return False

    print("[Build] Building SerializedFile...")
    try:
        sf = build_serialized_file(font_bytes)
        print(f"  SerializedFile: {len(sf):,} bytes")
    except Exception as e:
        print(f"  build_serialized_file failed: {e}")
        return False

    print("[Build] Wrapping in UnityFS...")
    try:
        bundle = build_unityfs(sf)
        out_bundle.write_bytes(bundle)
        print(f"  [OK] {out_bundle}  ({out_bundle.stat().st_size:,} bytes)")
        return True
    except Exception as e:
        print(f"  build_unityfs failed: {e}")
        return False

# ─── 메인 ────────────────────────────────────────────────────────────────────
success = False

# 1. 템플릿 직접 지정
if tmpl_arg and tmpl_arg.exists():
    print(f"[1] Template: {tmpl_arg}")
    success = inject_into_template(str(tmpl_arg))

# 2. ADB 자동 pull
if not success:
    print("[2] ADB pull...")
    tmpl = adb_pull_template()
    if tmpl:
        success = inject_into_template(tmpl)
        try: os.unlink(tmpl)
        except: pass

# 3. 처음부터 생성
if not success:
    print("[3] Create from scratch...")
    success = create_from_scratch()

print()
if success:
    sz = out_bundle.stat().st_size
    print(f"[SUCCESS] {out_bundle}  ({sz:,} bytes)")
    game_dir = f"/data/user/0/{GAME_PKG}/files/hoshimi-local/local-files"
    print()
    print("Deploy:")
    print(f"  adb push \"{out_bundle}\" /sdcard/Download/korean_font.bundle")
    print(f"  adb shell \"cp /sdcard/Download/korean_font.bundle {game_dir}/korean_font.bundle\"")
else:
    print("[FAILED] All methods failed.")
    print()
    print("Manual template:")
    print(f"  adb shell 'find /sdcard/Android/data/{GAME_PKG} -name \"*.bundle\"'")
    print(f"  adb pull <path> template.bundle")
    print(f"  python {Path(__file__).name} \"{otf_path}\" \"{out_bundle}\" template.bundle")
