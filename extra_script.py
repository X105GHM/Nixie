Import("env") # type: ignore
import os
import shutil
import zipfile
import subprocess
import hashlib
import json
from datetime import datetime
from SCons.Script import AlwaysBuild # type: ignore

output_dir = os.path.join(env["PROJECT_DIR"], "bin") # type: ignore
version_file = os.path.join(output_dir, "version.txt")

if not os.path.exists(output_dir):
    os.makedirs(output_dir)
    print("📂 Output directory created:", output_dir)

def sha256sum(filepath):
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            h.update(chunk)
    return h.hexdigest()

def get_next_version():
    base = "Nixie_V.6.0."
    if os.path.exists(version_file):
        with open(version_file, "r") as vf:
            last = vf.read().strip()
            try:
                n = int(last.split(".")[-1]) + 1
            except:
                n = 1
    else:
        n = 0
    version = f"{base}{n}"
    with open(version_file, "w") as vf:
        vf.write(version)
    return version

def write_manifest(version, firmware_path, spiffs_path):
    now = datetime.now()
    manifest = {
        "version": version,
        "timestamp": now.strftime("%Y-%m-%d %H:%M:%S"),
        "files": {
            "firmware.bin": sha256sum(firmware_path),
            "spiffs.bin": sha256sum(spiffs_path)
        }
    }
    manifest_path = os.path.join(output_dir, "manifest.json")
    with open(manifest_path, "w") as mf:
        json.dump(manifest, mf, indent=2)
    print("📝 Manifest written to", manifest_path)

def create_zip_with_timestamp(firmware_path, spiffs_path):
    now = datetime.now()
    timestamp = now.strftime("%Y%m%d_%H%M")
    zip_name = f"ota_package_{timestamp}.zip"
    zip_path = os.path.join(output_dir, zip_name)
    with zipfile.ZipFile(zip_path, 'w') as zipf:
        zipf.write(firmware_path, arcname="firmware.bin")
        zipf.write(spiffs_path, arcname="spiffs.bin")
    print(f"📦 Created OTA ZIP archive: {zip_name}")

def build_package(target, source, env):
    print("📦 Running full build for firmware + spiffs...")

    # Baue Firmware
    subprocess.run(["pio", "run"], check=True)
    firmware_src = os.path.join(env.subst("$BUILD_DIR"), f"{env.subst('$PROGNAME')}.bin")
    firmware_dst = os.path.join(output_dir, "firmware.bin")
    if os.path.exists(firmware_src):
        shutil.copy(firmware_src, firmware_dst)
        print("✅ Firmware copied to", firmware_dst)
    else:
        print("❌ firmware.bin not found")
        return

    # Baue SPIFFS
    subprocess.run(["pio", "run", "--target", "buildfs"], check=True)
    spiffs_src = os.path.join(env.subst("$BUILD_DIR"), "spiffs.bin")
    spiffs_dst = os.path.join(output_dir, "spiffs.bin")
    if os.path.exists(spiffs_src):
        shutil.copy(spiffs_src, spiffs_dst)
        print("✅ SPIFFS copied to", spiffs_dst)
    else:
        print("❌ spiffs.bin not found")
        return

    # Version & Manifest
    version = get_next_version()
    write_manifest(version, firmware_dst, spiffs_dst)

    # ZIP (nur Archiv)
    create_zip_with_timestamp(firmware_dst, spiffs_dst)

    print("✅ OTA package build complete")

AlwaysBuild(env.Alias("package", None, build_package)) # type: ignore
