import sys
import os
import shutil

build_dir   = sys.argv[1]
install_dir = sys.argv[2]

os.makedirs(install_dir, exist_ok=True)

for entry in sorted(os.listdir(build_dir)):
    if not entry.endswith('.vst3'):
        continue
    src = os.path.join(build_dir, entry)
    if not os.path.isdir(src):
        continue
    dst = os.path.join(install_dir, entry)
    if os.path.exists(dst):
        shutil.rmtree(dst)
    shutil.copytree(src, dst)
    print('Installing', entry, '->', dst)
