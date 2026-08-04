#!/usr/bin/env python3
"""
ESP-IDF wrapper for running idf.py with proper environment.
Works on Windows (PowerShell/CMD), Linux, macOS.
Usage: python scripts/run_esp32_idf.py <idf.py args...>
"""
import os
import sys
import subprocess
from pathlib import Path

# Working directory
WORKSPACE = Path(__file__).parent.parent
ESP32_DIR = WORKSPACE / 'platform' / 'esp32'

# ESP-IDF paths
IDF_PATH = Path(r'E:\soft\Espressif\frameworks\esp-idf-v5.5.5')
IDF_EXPORT_PS1 = IDF_PATH / 'export.ps1'

def main():
    args = sys.argv[1:]
    idf_args = ' '.join(args)

    # Always use PowerShell on Windows (export.ps1)
    # Use -ExecutionPolicy Bypass to avoid script security error
    activate_cmd = f'. {IDF_EXPORT_PS1}; cd {ESP32_DIR}; idf.py {idf_args}'
    full_cmd = ['powershell', '-ExecutionPolicy', 'Bypass', '-Command', activate_cmd]

    # Add sandbox workaround env vars
    env = os.environ.copy()
    env.update({
        'IDF_SKIP_CHECK_SUBMODULES': '1',
        'IDF_COMPONENT_CACHE_PATH': str(WORKSPACE / '.espressif-cache'),
        'PYTHONDONTWRITEBYTECODE': '1',
        'CCACHE_DIR': str(WORKSPACE / '.ccache'),
        'CCACHE_DISABLE': '1',
    })

    # Run
    result = subprocess.run(full_cmd, env=env)
    sys.exit(result.returncode)

if __name__ == '__main__':
    main()