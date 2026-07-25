#!/usr/bin/env python3
"""Pack all built .qvm files from a directory into a pk3 (vm/<name>.qvm layout)"""
import sys
import zipfile
import os
import glob

if len(sys.argv) != 3:
    print("Usage: python create_pk3.py <vm_dir> <pk3_file>")
    sys.exit(1)

vm_dir = sys.argv[1]
pk3_file = sys.argv[2]

qvm_files = sorted(glob.glob(os.path.join(vm_dir, "*.qvm")))
if not qvm_files:
    print(f"ERROR: No .qvm files found in {vm_dir}")
    sys.exit(1)

try:
    with zipfile.ZipFile(pk3_file, 'w', zipfile.ZIP_DEFLATED) as z:
        for qvm_file in qvm_files:
            z.write(qvm_file, arcname=f"vm/{os.path.basename(qvm_file)}")
    print(f"Created: {pk3_file}")
    for qvm_file in qvm_files:
        print(f"  + vm/{os.path.basename(qvm_file)}")
except Exception as e:
    print(f"ERROR: Failed to create pk3: {e}")
    sys.exit(1)
