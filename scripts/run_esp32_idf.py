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
import shlex
import shutil
import subprocess
from pathlib import Path

# Working directory
WORKSPACE = Path(__file__).parent.parent
ESP32_DIR = WORKSPACE / 'platform' / 'esp32'
BUILD_DIR = ESP32_DIR / 'build'
QEMU_PORT = '5555'
IS_WIN = sys.platform.startswith('win')


def is_mingw():
    """Check if running in MinGW/Git Bash."""
    return 'MSYSTEM' in os.environ


def _first_existing_dir(*candidates):
    for c in candidates:
        if c and Path(c).is_dir():
            return Path(c)
    return None


def _first_existing_file(*candidates):
    for c in candidates:
        if c and Path(c).is_file():
            return Path(c)
    return None


def resolve_idf_path():
    """Locate ESP-IDF root (contains tools/idf.py + export.sh/ps1)."""
    env = _first_existing_dir(os.environ.get('ESP_IDF_PATH'),
                              os.environ.get('IDF_PATH'))
    if env and (env / 'tools' / 'idf.py').is_file():
        return env

    candidates = []
    if IS_WIN:
        candidates.append(Path(r'E:\soft\Espressif\frameworks\esp-idf-v5.5.5'))
        candidates.append(Path(r'E:\soft\Espressif\framework\esp-idf-v5.5.5'))
    else:
        home = Path.home()
        espressif = home / '.espressif'
        if espressif.is_dir():
            # Prefer newest v*/esp-idf (e.g. v6.0.2/esp-idf)
            candidates.extend(sorted(espressif.glob('v*/esp-idf'), reverse=True))
        candidates.extend([
            home / 'esp' / 'esp-idf',
            Path('/opt/esp/esp-idf'),
        ])

    for c in candidates:
        if (c / 'tools' / 'idf.py').is_file():
            return c
    return None


def resolve_idf_tools_path():
    env = os.environ.get('IDF_TOOLS_PATH') or os.environ.get('ESP_IDF_TOOLS_PATH')
    if env and Path(env).is_dir():
        return Path(env)
    if IS_WIN:
        win = Path(r'E:\soft\Espressif\tools')
        if win.is_dir():
            return win
        return Path.home() / '.espressif'

    # EIM layout: toolchains + activate_idf_*.sh live under ~/.espressif/tools
    eim_tools = Path.home() / '.espressif' / 'tools'
    if eim_tools.is_dir() and (
            any(eim_tools.glob('activate_idf_*.sh')) or
            (eim_tools / 'riscv32-esp-elf').is_dir() or
            (eim_tools / 'python').is_dir()):
        return eim_tools

    # Classic idf_tools install uses ~/.espressif as IDF_TOOLS_PATH
    return Path.home() / '.espressif'


def resolve_idf_python(tools_path):
    """Python used for esptool / idf_monitor / spiffsgen."""
    env_py = os.environ.get('ESP_IDF_PYTHON')
    if env_py and Path(env_py).is_file():
        return env_py

    if IS_WIN:
        win = Path(r'E:\soft\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe')
        if win.is_file():
            return str(win)
        for pat in ('python_env/*/Scripts/python.exe',
                    'python/*/venv/Scripts/python.exe',
                    'tools/python/*/venv/Scripts/python.exe'):
            hits = sorted(tools_path.glob(pat), reverse=True)
            if hits:
                return str(hits[0])
    else:
        for pat in ('python/*/venv/bin/python',
                    'python_env/*/bin/python',
                    'tools/python/*/venv/bin/python'):
            hits = sorted(tools_path.glob(pat), reverse=True)
            if hits:
                return str(hits[0])

    return sys.executable


def resolve_toolchain_dir(tools_path):
    env = os.environ.get('ESP_IDF_TOOLCHAIN')
    if env and os.path.isdir(env):
        return env

    search_roots = [tools_path, tools_path / 'tools']
    for root in search_roots:
        if not root.is_dir():
            continue
        hits = sorted(root.glob('riscv32-esp-elf/*/riscv32-esp-elf/bin'), reverse=True)
        if hits:
            return str(hits[0])
    return ''


def find_eim_activate(idf_path):
    """Espressif IDE Manager activate script, if present."""
    tools = Path.home() / '.espressif' / 'tools'
    if not tools.is_dir():
        return None
    # idf_path like ~/.espressif/v6.0.2/esp-idf → activate_idf_v6.0.2.sh
    ver = idf_path.parent.name if idf_path else ''
    if ver.startswith('v'):
        specific = tools / f'activate_idf_{ver}.sh'
        if specific.is_file():
            return specific
    hits = sorted(tools.glob('activate_idf_v*.sh'), reverse=True)
    return hits[0] if hits else None


def resolve_default_port():
    return os.environ.get('ESP32_PORT') or ('COM3' if IS_WIN else '/dev/ttyUSB0')


IDF_PATH = resolve_idf_path()
IDF_TOOLS_PATH = resolve_idf_tools_path()
IDF_PYTHON = resolve_idf_python(IDF_TOOLS_PATH)
TOOLCHAIN_DIR = resolve_toolchain_dir(IDF_TOOLS_PATH)


def find_qemu():
    """Locate qemu-system-riscv32 executable."""
    env_qemu = os.environ.get('ESP_IDF_QEMU')
    if env_qemu and Path(env_qemu).is_file():
        return env_qemu

    which = shutil.which('qemu-system-riscv32')
    if which:
        return which

    roots = [IDF_TOOLS_PATH, IDF_TOOLS_PATH / 'tools']
    if IS_WIN:
        roots.append(Path(r'E:\soft\Espressif\tools'))
    roots.append(Path.home() / '.espressif' / 'tools')

    for tools in roots:
        if not tools.is_dir():
            continue
        for pat in ('qemu-riscv32/*/qemu/bin/qemu-system-riscv32',
                    'qemu-riscv32/*/qemu/bin/qemu-system-riscv32.exe',
                    'qemu-riscv32/*/qemu/bin/qemu-system-riscv32*'):
            hits = [h for h in sorted(tools.glob(pat), reverse=True)
                    if h.is_file() and not h.name.endswith(('.debug', '.pdb'))]
            if hits:
                return str(hits[0])
    return None


def base_env():
    """Sandbox workaround env vars + toolchain PATH."""
    if not IDF_PATH:
        print("ERROR: ESP-IDF not found. Set ESP_IDF_PATH/IDF_PATH or install under ~/.espressif/v*/esp-idf",
              file=sys.stderr)
        sys.exit(1)

    env = os.environ.copy()
    env.update({
        'IDF_SKIP_CHECK_SUBMODULES': '1',
        'IDF_COMPONENT_CACHE_PATH': str(WORKSPACE / '.espressif-cache'),
        'PYTHONDONTWRITEBYTECODE': '1',
        'CCACHE_DIR': str(WORKSPACE / '.ccache'),
        'CCACHE_DISABLE': '1',
        'IDF_PATH': str(IDF_PATH),
        'IDF_TOOLS_PATH': str(IDF_TOOLS_PATH),
    })
    # Prepend toolchain to PATH so monitor can find addr2line
    if TOOLCHAIN_DIR and os.path.isdir(TOOLCHAIN_DIR):
        env['PATH'] = TOOLCHAIN_DIR + os.pathsep + env.get('PATH', '')
    if is_mingw():
        env.pop('MSYSTEM', None)
        env.pop('MSYS', None)
    return env


def run_idf(env, *args):
    """Run idf.py after activating ESP-IDF export/activate script."""
    if IS_WIN and not is_mingw():
        export_ps1 = IDF_PATH / 'export.ps1'
        if not export_ps1.is_file():
            print(f"ERROR: missing {export_ps1}", file=sys.stderr)
            return 1
        idf_args = ' '.join(args)
        activate_cmd = f'. {export_ps1}; cd {ESP32_DIR}; idf.py {idf_args}'
        full_cmd = ['powershell', '-ExecutionPolicy', 'Bypass', '-Command', activate_cmd]
        return subprocess.run(full_cmd, env=env).returncode

    idf_args = ' '.join(shlex.quote(a) for a in args)
    eim_activate = find_eim_activate(IDF_PATH)
    if eim_activate:
        # EIM activate may exit non-zero (e.g. optional `eim select` fails);
        # still apply env, then run idf.py.
        cmd = (
            f'source {shlex.quote(str(eim_activate))}; '
            f'cd {shlex.quote(str(ESP32_DIR))} && '
            f'idf.py {idf_args}'
        )
        return subprocess.run(['bash', '-c', cmd], env=env).returncode

    export_sh = IDF_PATH / 'export.sh'
    if not export_sh.is_file():
        print(f"ERROR: missing {export_sh}", file=sys.stderr)
        return 1
    # Classic install: point export.sh at the resolved venv when present
    py = Path(IDF_PYTHON)
    venv = py.parent.parent  # .../venv/{bin|Scripts}/python
    if venv.name == 'venv' or (venv / 'pyvenv.cfg').is_file():
        env = dict(env)
        env['IDF_PYTHON_ENV_PATH'] = str(venv)
    cmd = (
        f'source {shlex.quote(str(export_sh))}; '
        f'cd {shlex.quote(str(ESP32_DIR))} && '
        f'idf.py {idf_args}'
    )
    return subprocess.run(['bash', '-c', cmd], env=env).returncode


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
        print("ERROR: qemu-system-riscv32 not found.", file=sys.stderr)
        print("  Install:  python $IDF_PATH/tools/idf_tools.py install qemu-riscv32",
              file=sys.stderr)
        print("  Or set:   ESP_IDF_QEMU=/path/to/qemu-system-riscv32", file=sys.stderr)
        sys.exit(1)

    # qemu_flash.bin is prepared by main() via merge_qemu_flash()
    flash = BUILD_DIR / 'qemu_flash.bin'
    if not flash.exists():
        print("ERROR: qemu_flash.bin missing; merge_qemu_flash was not run", file=sys.stderr)
        sys.exit(1)
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
    port = resolve_default_port()
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
    port = resolve_default_port()
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
    print(f"ESP-IDF: {IDF_PATH}", flush=True)
    print(f"IDF tools: {IDF_TOOLS_PATH}", flush=True)
    print(f"IDF python: {IDF_PYTHON}", flush=True)
    env = base_env()

    # QEMU firmware build: set YUI_ESP32_QEMU so main skips LCD/touch init
    if len(args) >= 1 and args[0] == 'build-qemu':
        env['YUI_ESP32_QEMU'] = '1'
        # Force CMake reconfigure so the compile definition is applied
        sys.exit(run_idf(env, 'reconfigure', 'build'))

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

    # QEMU: always merge font + SPIFFS into qemu_flash.bin first.
    # idf.py qemu only merges bootloader/app/partition by default, so without
    # this step the font and spiffs partitions are empty (mount -10025 / no font).
    if len(args) >= 1 and args[0] == 'qemu':
        merge_qemu_flash(env)
        flash = BUILD_DIR / 'qemu_flash.bin'
        if '--graphics' not in args:
            # Headless: custom foreground launch (avoids idf.py bg-spawn issues)
            run_qemu_headless(env)
            return
        # Graphics: hand the pre-merged image to idf.py
        rest = [a for a in args[1:] if a != '--graphics']
        sys.exit(run_idf(env, 'qemu', '--graphics',
                         '--flash-file', str(flash), *rest))

    # Everything else goes through idf.py
    sys.exit(run_idf(env, *args))


if __name__ == '__main__':
    main()
