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

def merge_qemu_flash(env):
    """Generate qemu_flash.bin from bootloader/app/partition binaries."""
    cmd = [IDF_PYTHON, '-m', 'esptool', '--chip', 'esp32c3', 'merge_bin',
           '--output', str(BUILD_DIR / 'qemu_flash.bin'),
           '--fill-flash-size', '2MB', '--flash_mode', 'dio',
           '--flash_freq', '80m', '--flash_size', '2MB',
           '0x0', str(BUILD_DIR / 'bootloader' / 'bootloader.bin'),
           '0x10000', str(BUILD_DIR / 'yui_esp32.bin'),
           '0x8000', str(BUILD_DIR / 'partition_table' / 'partition-table.bin')]
    result = subprocess.run(cmd, env=env, cwd=str(ESP32_DIR))
    if result.returncode != 0:
        print(f"ERROR: merge_bin failed (rc={result.returncode})", file=sys.stderr)
        sys.exit(result.returncode)

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

def main():
    args = sys.argv[1:]
    env = base_env()

    # Headless QEMU: `qemu monitor` (no --graphics)
    if len(args) >= 1 and args[0] == 'qemu' and '--graphics' not in args:
        run_qemu_headless(env)
        return

    # Everything else goes through idf.py
    sys.exit(run_idf(env, *args))

if __name__ == '__main__':
    main()
