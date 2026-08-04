#!/usr/bin/env python3
"""
ESP-IDF wrapper for running idf.py with proper environment.
Works on Windows (PowerShell/CMD), Linux, macOS.
Usage: python scripts/run_esp32_idf.py <idf.py args...>

Special handling for headless QEMU (`qemu monitor` without --graphics):
ESP-IDF's idf.py spawns QEMU as a background process, which gets blocked in
sandboxed environments. This script launches QEMU as a foreground subprocess
itself, then runs idf_monitor to connect.
"""
import os
import sys
import time
import glob
import subprocess
from pathlib import Path

# Working directory
WORKSPACE = Path(__file__).parent.parent
ESP32_DIR = WORKSPACE / 'platform' / 'esp32'
BUILD_DIR = ESP32_DIR / 'build'

# ESP-IDF paths (override via env for custom installs)
IDF_PATH = Path(os.environ.get('ESP_IDF_PATH', r'E:\soft\Espressif\frameworks\esp-idf-v5.5.5'))
IDF_PYTHON = os.environ.get('ESP_IDF_PYTHON',
                            r'E:\soft\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe')
IDF_EXPORT_PS1 = IDF_PATH / 'export.ps1'
QEMU_PORT = '5555'

# Toolchain prefix directory (for addr2line in monitor)
TOOLCHAIN_DIR = os.environ.get(
    'ESP_IDF_TOOLCHAIN',
    r'E:\soft\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin')

def is_mingw():
    """Check if running in MinGW/Git Bash."""
    return 'MSYSTEM' in os.environ

def find_qemu():
    """Locate qemu-system-riscv32 executable."""
    env_qemu = os.environ.get('ESP_IDF_QEMU')
    if env_qemu and Path(env_qemu).is_file():
        return env_qemu
    tools = Path(r'E:\soft\Espressif\tools')
    for pat in ('qemu-riscv32/*/qemu/bin/qemu-system-riscv32*.exe',
                'qemu-riscv32/*/qemu/bin/qemu-system-riscv32*'):
        hits = list(tools.glob(pat))
        if hits:
            return str(hits[0])
    return None

def base_env():
    """Sandbox workaround env vars + toolchain PATH."""
    env = os.environ.copy()
    env.update({
        'IDF_SKIP_CHECK_SUBMODULES': '1',
        'IDF_COMPONENT_CACHE_PATH': str(WORKSPACE / '.espressif-cache'),
        'PYTHONDONTWRITEBYTECODE': '1',
        'CCACHE_DIR': str(WORKSPACE / '.ccache'),
        'CCACHE_DISABLE': '1',
        'IDF_PATH': str(IDF_PATH),
    })
    # Prepend toolchain to PATH so monitor can find addr2line
    if os.path.isdir(TOOLCHAIN_DIR):
        env['PATH'] = TOOLCHAIN_DIR + os.pathsep + env.get('PATH', '')
    if is_mingw():
        env.pop('MSYSTEM', None)
        env.pop('MSYS', None)
    return env

def run_idf(env, *args):
    """Run idf.py via PowerShell export.ps1 (works in PowerShell & MinGW)."""
    idf_args = ' '.join(args)
    activate_cmd = f'. {IDF_EXPORT_PS1}; cd {ESP32_DIR}; idf.py {idf_args}'
    full_cmd = ['powershell', '-ExecutionPolicy', 'Bypass', '-Command', activate_cmd]
    result = subprocess.run(full_cmd, env=env)
    return result.returncode

def read_partition_table(env):
    """Parse build/partition_table/partition-table.bin -> {name: (offset, size)}.
    Entry layout (32 bytes): magic 0x50AA (2), type (1), subtype (1),
    offset (4), size (4), name (16), flags (4)."""
    ptable = BUILD_DIR / 'partition_table' / 'partition-table.bin'
    if not ptable.exists():
        return {}
    data = ptable.read_bytes()
    parts = {}
    for i in range(0, len(data) - 32 + 1, 32):
        if int.from_bytes(data[i:i + 2], 'little') != 0x50AA:  # magic
            break
        offset = int.from_bytes(data[i + 4:i + 8], 'little')
        size = int.from_bytes(data[i + 8:i + 12], 'little')
        name = data[i + 12:i + 28].split(b'\x00')[0].decode('utf-8', 'replace')
        parts[name] = (offset, size)
    return parts

def merge_qemu_flash(env):
    """Generate qemu_flash.bin (4MB) from bootloader/app/partition + font + spiffs."""
    cmd = [IDF_PYTHON, '-m', 'esptool', '--chip', 'esp32c3', 'merge_bin',
           '--output', str(BUILD_DIR / 'qemu_flash.bin'),
           '--fill-flash-size', '2MB', '--flash_mode', 'dio',
           '--flash_freq', '80m', '--flash_size', '2MB',
           '0x0', str(BUILD_DIR / 'bootloader' / 'bootloader.bin'),
           '0x10000', str(BUILD_DIR / 'yui_esp32.bin'),
           '0x8000', str(BUILD_DIR / 'partition_table' / 'partition-table.bin')]

    parts = read_partition_table(env)

    # Add font partition data if available (from build/font-subset.ttf)
    font_bin = BUILD_DIR / 'font-subset.ttf'
    if font_bin.exists():
        if 'font' in parts:
            font_off, _ = parts['font']
            cmd += [f'0x{font_off:x}', str(font_bin)]
            print(f"Merge font '{font_bin.name}' at offset 0x{font_off:x}", flush=True)
        else:
            print("WARN: no 'font' partition in partition table, skipping font merge", file=sys.stderr)

    # Add SPIFFS partition data (watch-os apps) if available
    spiffs_img = BUILD_DIR / 'watch-os.img'
    if spiffs_img.exists():
        if 'spiffs' in parts:
            spiffs_off, _ = parts['spiffs']
            cmd += [f'0x{spiffs_off:x}', str(spiffs_img)]
            print(f"Merge SPIFFS '{spiffs_img.name}' at offset 0x{spiffs_off:x}", flush=True)
        else:
            print("WARN: no 'spiffs' partition in partition table, skipping spiffs merge", file=sys.stderr)

    result = subprocess.run(cmd, env=env, cwd=str(ESP32_DIR))
    if result.returncode != 0:
        print(f"ERROR: merge_bin failed (rc={result.returncode})", file=sys.stderr)
        sys.exit(result.returncode)

def make_spiffs(env):
    """Build SPIFFS image into build/watch-os.img.

    Layout (SPIFFS root):
      watch-os/  - watch-os app tree (apps, themes, store, ...)
      lib/       - shared JS libs referenced via "../lib/.." (router.js, theme.js)

    Constraints:
      - SPIFFS obj name limit is 32 chars; deep paths are skipped with a warning.
      - app/assets (large font files) is excluded - embedded fonts are served
        from the 'font' partition, not from SPIFFS.
    """
    import shutil
    staging = BUILD_DIR / 'spiffs-staging'
    if staging.exists():
        shutil.rmtree(staging)

    # 1. watch-os app tree (SPIFFS root = apps/, lib/, themes/, ...)
    src = WORKSPACE / 'app' / 'watch-os'
    if not src.is_dir():
        print("ERROR: no app/watch-os directory", file=sys.stderr)
        sys.exit(1)
    skipped = []
    for dirpath, dirnames, filenames in os.walk(src):
        rel_dir = os.path.relpath(dirpath, src).replace('\\', '/')
        for fn in filenames:
            img_path = f"{rel_dir}/{fn}" if rel_dir != '.' else fn
            # spiffsgen adds '/' prefix, so max allowed is 31 chars for img_path
            if len(img_path) >= 32:
                skipped.append(img_path)
                continue
            full = Path(dirpath) / fn
            out = staging / img_path.replace('/', os.sep)
            out.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(full, out)
    if skipped:
        print(f"WARN: {len(skipped)} file(s) skipped (SPIFFS path > 32 chars):", flush=True)
        for p in skipped:
            print(f"  - {p}", flush=True)

    # 2. shared lib files referenced by app.json as "../lib/*"
    #    (mirrored at SPIFFS root as lib/)
    lib_src = WORKSPACE / 'app' / 'lib'
    lib_dst = staging / 'lib'
    if lib_src.is_dir():
        lib_dst.mkdir(parents=True, exist_ok=True)
        for fn in ('router.js', 'theme.js'):
            f = lib_src / fn
            if f.is_file():
                shutil.copy2(f, lib_dst / fn)

    parts = read_partition_table(env)
    spiffs_size = parts['spiffs'][1] if 'spiffs' in parts else 0x80000

    out = BUILD_DIR / 'watch-os.img'
    cmd = [IDF_PYTHON, str(IDF_PATH / 'components' / 'spiffs' / 'spiffsgen.py'),
           str(spiffs_size), str(staging), str(out)]
    print(f"Building SPIFFS image ({spiffs_size} bytes): app/watch-os + app/lib", flush=True)
    result = subprocess.run(cmd, env=env, cwd=str(ESP32_DIR))
    if result.returncode != 0:
        print(f"ERROR: spiffsgen failed (rc={result.returncode})", file=sys.stderr)
        sys.exit(result.returncode)
    print(f"OK: {out} ({out.stat().st_size} bytes)", flush=True)

def run_qemu_headless(env):
    """Manual QEMU launch + monitor (avoids idf.py background-spawn issue)."""
    qemu = find_qemu()
    if not qemu:
        print("ERROR: qemu-system-riscv32 not found. Install via idf_tools.py", file=sys.stderr)
        sys.exit(1)

    # Make sure flash image is up to date
    merge_qemu_flash(env)

    flash = BUILD_DIR / 'qemu_flash.bin'
    efuse = BUILD_DIR / 'qemu_efuse.bin'
    if not efuse.exists():
        efuse = Path(os.devnull)  # efuse optional

    qemu_cmd = [
        qemu, '-M', 'esp32c3',
        '-drive', f'file={flash},if=mtd,format=raw',
        '-drive', f'file={efuse},if=none,format=raw,id=efuse',
        '-global', 'driver=nvram.esp32c3.efuse,property=drive,value=efuse',
        '-global', 'driver=timer.esp32c3.timg,property=wdt_disable,value=true',
        '-nic', 'user,model=open_eth',
        '-nographic', '-serial', f'tcp::{QEMU_PORT},server',
    ]
    print(f"Launching QEMU: {' '.join(qemu_cmd)}", flush=True)
    qemu_proc = subprocess.Popen(qemu_cmd, cwd=str(ESP32_DIR))

    # Wait for QEMU to listen on the serial port
    time.sleep(2)

    monitor_cmd = [
        IDF_PYTHON, str(IDF_PATH / 'tools' / 'idf_monitor.py'),
        '-p', f'socket://localhost:{QEMU_PORT}', '-b', '115200',
        '--toolchain-prefix', 'riscv32-esp-elf-',
        '--target', 'esp32c3', '--revision', '3',
        '--decode-panic', 'backtrace',
        str(BUILD_DIR / 'yui_esp32.elf'),
        str(BUILD_DIR / 'bootloader' / 'bootloader.elf'),
    ]
    try:
        result = subprocess.run(monitor_cmd, env=env, cwd=str(ESP32_DIR))
    finally:
        qemu_proc.terminate()
        try:
            qemu_proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            qemu_proc.kill()
    sys.exit(result.returncode)

def run_write_font(env):
    """Flash the subset font into the 'font' data partition on real hardware."""
    port = os.environ.get('ESP32_PORT', 'COM3')
    font_bin = BUILD_DIR / 'font-subset.ttf'
    if not font_bin.exists():
        print("ERROR: font-subset.ttf not found, run 'make esp32-font' first", file=sys.stderr)
        sys.exit(1)
    parts = read_partition_table(env)
    if 'font' not in parts:
        print("ERROR: no 'font' partition in partition table", file=sys.stderr)
        sys.exit(1)
    off, _ = parts['font']
    print(f"Flashing font to {port} at offset 0x{off:x} ...", flush=True)
    cmd = [IDF_PYTHON, '-m', 'esptool', '--chip', 'esp32c3', '-p', port,
           'write_flash', f'0x{off:x}', str(font_bin)]
    result = subprocess.run(cmd, env=env, cwd=str(ESP32_DIR))
    sys.exit(result.returncode)

def run_write_spiffs(env):
    """Flash the SPIFFS image into the 'spiffs' data partition on real hardware."""
    port = os.environ.get('ESP32_PORT', 'COM3')
    img = BUILD_DIR / 'watch-os.img'
    if not img.exists():
        print("ERROR: watch-os.img not found, run 'make esp32-spiffs' first", file=sys.stderr)
        sys.exit(1)
    parts = read_partition_table(env)
    if 'spiffs' not in parts:
        print("ERROR: no 'spiffs' partition in partition table", file=sys.stderr)
        sys.exit(1)
    off, _ = parts['spiffs']
    print(f"Flashing SPIFFS to {port} at offset 0x{off:x} ...", flush=True)
    cmd = [IDF_PYTHON, '-m', 'esptool', '--chip', 'esp32c3', '-p', port,
           'write_flash', f'0x{off:x}', str(img)]
    result = subprocess.run(cmd, env=env, cwd=str(ESP32_DIR))
    sys.exit(result.returncode)

def main():
    args = sys.argv[1:]
    env = base_env()

    # Build SPIFFS image from app/watch-os + app/assets
    if len(args) >= 1 and args[0] == 'make-spiffs':
        make_spiffs(env)
        return

    # Write subset font into the font partition (real hardware)
    if len(args) >= 1 and args[0] == 'write-font':
        run_write_font(env)
        return

    # Write SPIFFS image into the spiffs partition (real hardware)
    if len(args) >= 1 and args[0] == 'write-spiffs':
        run_write_spiffs(env)
        return

    # Headless QEMU: `qemu monitor` (no --graphics)
    if len(args) >= 1 and args[0] == 'qemu' and '--graphics' not in args:
        run_qemu_headless(env)
        return

    # Everything else goes through idf.py
    sys.exit(run_idf(env, *args))

if __name__ == '__main__':
    main()
