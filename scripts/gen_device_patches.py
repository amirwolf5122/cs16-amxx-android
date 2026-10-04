#!/usr/bin/env python3
"""gen_device_patches.py — generate the device-fix patch series
(patches/<name>/) by diffing the in-tree fork against a fresh upstream
clone. Output = git-style unified
patches that apply with `git apply -p1` inside a fresh upstream clone:

  a/<relpath>  b/<relpath>            modified files
  a/dev/null   b/<relpath>            new files (added by us)

Junk (build outputs, waf state, .git) is excluded.
"""
import subprocess, sys, os, shutil, tempfile

ROOT = "/home/z/work/cs16-amxx-android"
JUNK_DIRS = {".git", "build", "build-full", ".waf3-2.1.9-94748e4140e2938f04aabdd771338daa",
             ".lock-waf_linux_build", "3rdparty"}  # 3rdparty handled by upstream submodules
JUNK_SUFFIX = (".o", ".so", ".a", ".dex", ".apk", ".log", ".zip", ".wad", ".bsp", ".mdl")

SPECS = [
    ("xash3d",     "/tmp/upstream-xash3d",     f"{ROOT}/xash3d-fwgs-master"),
    ("cs16client", "/tmp/upstream-cs16client", f"{ROOT}/cs16-client-main"),
]

def junk(relpath):
    parts = relpath.split("/")
    if any(p in JUNK_DIRS for p in parts):
        return True
    return relpath.endswith(JUNK_SUFFIX)

def walk(root):
    out = {}
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in JUNK_DIRS]
        for f in filenames:
            full = os.path.join(dirpath, f)
            rel = os.path.relpath(full, root)
            if junk(rel):
                continue
            out[rel] = full
    return out

def is_text(path):
    try:
        with open(path, "rb") as fh:
            chunk = fh.read(8192)
        return b"\0" not in chunk
    except OSError:
        return False

for name, upstream, intree in SPECS:
    if not os.path.isdir(upstream):
        print(f"SKIP {name}: upstream clone missing at {upstream}")
        continue
    print(f"== {name}")
    up_files = walk(upstream)
    in_files = walk(intree)

    outdir = os.path.join(ROOT, "patches", name)
    os.makedirs(outdir, exist_ok=True)
    # clear old generated series
    for f in os.listdir(outdir):
        if f.endswith(".patch"):
            os.remove(os.path.join(outdir, f))

    idx = 0
    parts = []
    rel_all = sorted(set(up_files) | set(in_files))
    for rel in rel_all:
        if rel in up_files and rel in in_files:
            if open(up_files[rel], "rb").read() == open(in_files[rel], "rb").read():
                continue
            if not (is_text(up_files[rel]) and is_text(in_files[rel])):
                continue  # binary device assets are shipped via the repo, not patches
            r = subprocess.run(["diff", "-u", up_files[rel], in_files[rel]],
                               capture_output=True, text=True)
            if r.returncode != 1:  # 1 = differences found
                continue
            d = r.stdout
            d = d.replace(f"--- {up_files[rel]}", f"--- a/{rel}", 1)
            d = d.replace(f"+++ {in_files[rel]}", f"+++ b/{rel}", 1)
            parts.append(d)
        elif rel in in_files:  # new file added by our device work
            if not is_text(in_files[rel]):
                continue
            r = subprocess.run(["diff", "-u", "/dev/null", in_files[rel]],
                               capture_output=True, text=True)
            if r.returncode != 1:
                continue
            d = r.stdout
            d = d.replace("--- /dev/null", "--- a/dev/null", 1)
            d = d.replace(f"+++ {in_files[rel]}", f"+++ b/{rel}", 1)
            parts.append(d)
        # files only upstream (deleted by us) are ignored: the repo copy wins

    if parts:
        idx += 1
        blob = "".join(parts)
        with open(os.path.join(outdir, f"0001-device-fixes.patch"), "w") as fh:
            fh.write(blob)
        print(f"   wrote 0001-device-fixes.patch: {len(parts)} file diffs, {len(blob)//1024} KiB")
    else:
        # empty series == in-tree copy is identical to upstream
        with open(os.path.join(outdir, "0001-device-fixes.patch"), "w") as fh:
            fh.write("")
        print("   in-tree copy is identical to upstream (empty patch)")
print("done")
