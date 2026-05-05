#!/usr/bin/env python3
import os
import shutil
import zipfile
import hashlib
import json
from datetime import datetime

Import("env")  # type: ignore

PROJECT_DIR = env.subst("$PROJECT_DIR")  # type: ignore
BUILD_DIR = env.subst("$BUILD_DIR")      # type: ignore

OUTPUT_DIR = os.path.join(PROJECT_DIR, "bin")
VERSION_FILE = os.path.join(OUTPUT_DIR, "version.txt")

os.makedirs(OUTPUT_DIR, exist_ok=True)


def sha256sum(filepath):
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            h.update(chunk)
    return h.hexdigest()


def get_next_version():
    base = "Nixie_V.6.6."

    if os.path.exists(VERSION_FILE):
        with open(VERSION_FILE, "r", encoding="utf-8") as vf:
            last = vf.read().strip()
            try:
                n = int(last.split(".")[-1]) + 1
            except ValueError:
                n = 0
    else:
        n = 0

    version = f"{base}{n}"

    with open(VERSION_FILE, "w", encoding="utf-8") as vf:
        vf.write(version)

    return version


def write_manifest(version, firmware_path, spiffs_path):
    manifest = {
        "version": version,
        "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "files": {
            "firmware.bin": sha256sum(firmware_path),
            "spiffs.bin": sha256sum(spiffs_path)
        }
    }

    manifest_path = os.path.join(OUTPUT_DIR, "manifest.json")

    with open(manifest_path, "w", encoding="utf-8") as mf:
        json.dump(manifest, mf, indent=2)

    print(f"Manifest geschrieben nach {manifest_path}")


def create_zip_with_timestamp(firmware_path, spiffs_path):
    timestamp = datetime.now().strftime("%Y%m%d_%H%M")
    zip_name = f"ota_package_{timestamp}.zip"
    zip_path = os.path.join(OUTPUT_DIR, zip_name)

    with zipfile.ZipFile(zip_path, "w") as zipf:
        zipf.write(firmware_path, arcname="firmware.bin")
        zipf.write(spiffs_path, arcname="spiffs.bin")

    print(f"OTA-Archiv erstellt: {zip_name}")


def find_firmware_bin():
    candidates = []

    for name in os.listdir(BUILD_DIR):
        if not name.endswith(".bin"):
            continue

        if name in ("spiffs.bin", "littlefs.bin", "bootloader.bin", "partitions.bin"):
            continue

        candidates.append(name)

    if not candidates:
        return None

    # Meist ist firmware.bin oder projektname.bin richtig.
    if "firmware.bin" in candidates:
        return os.path.join(BUILD_DIR, "firmware.bin")

    return os.path.join(BUILD_DIR, candidates[0])


def package_existing_build(source, target, env):  # type: ignore
    firmware_src = find_firmware_bin()
    spiffs_src = os.path.join(BUILD_DIR, "spiffs.bin")

    if not firmware_src or not os.path.exists(firmware_src):
        print("Keine Firmware-Datei gefunden. OTA-Paket wird nicht erstellt.")
        return

    if not os.path.exists(spiffs_src):
        print(f"SPIFFS nicht gefunden: {spiffs_src}")
        print("Hinweis: Vorher `pio run -t buildfs` ausführen, wenn spiffs.bin ins OTA-Paket soll.")
        return

    firmware_dst = os.path.join(OUTPUT_DIR, "firmware.bin")
    spiffs_dst = os.path.join(OUTPUT_DIR, "spiffs.bin")

    shutil.copy(firmware_src, firmware_dst)
    shutil.copy(spiffs_src, spiffs_dst)

    print(f"firmware.bin & spiffs.bin kopiert nach {OUTPUT_DIR}")

    version = get_next_version()
    write_manifest(version, firmware_dst, spiffs_dst)
    create_zip_with_timestamp(firmware_dst, spiffs_dst)

    print("OTA-Paket-Build abgeschlossen")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", package_existing_build)  # type: ignore