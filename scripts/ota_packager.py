#!/usr/bin/env python3
import os
import shutil
import zipfile
import hashlib
import json
import struct
from datetime import datetime
from pathlib import Path

Import("env")  # type: ignore

PROJECT_DIR = env.subst("$PROJECT_DIR")  # type: ignore
BUILD_DIR = env.subst("$BUILD_DIR")      # type: ignore
PUBLIC_OUTPUT_DIR = os.path.join(PROJECT_DIR, "bin")
LOCAL_SECRETS_FILE = os.path.join(PROJECT_DIR, "src", "Config", "LocalSecrets.hpp")
# Release artifacts intentionally include the locally configured weather key.
OUTPUT_DIR = PUBLIC_OUTPUT_DIR
VERSION_FILE = os.path.join(PUBLIC_OUTPUT_DIR, "version.txt")

os.makedirs(OUTPUT_DIR, exist_ok=True)
if os.path.exists(LOCAL_SECRETS_FILE):
    print("Build mit lokaler Secret-Konfiguration: OTA-Artefakte werden nach bin/ geschrieben.")

def sha256sum(filepath):
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            h.update(chunk)
    return h.hexdigest()


def read_current_version():
    if not os.path.exists(VERSION_FILE):
        raise FileNotFoundError(f"Missing version file: {VERSION_FILE}")

    with open(VERSION_FILE, "r", encoding="utf-8") as vf:
        version = vf.read().strip()

    if not version:
        raise ValueError(f"Version file is empty: {VERSION_FILE}")

    return version


def read_firmware_version(firmware_path):
    # ESP image header (24 bytes), first segment header (8 bytes), then
    # esp_app_desc_t: magic/security/reserved (16 bytes), version (32 bytes).
    with open(firmware_path, "rb") as firmware:
        header = firmware.read(80)
    if len(header) != 80 or header[0] != 0xE9 or struct.unpack_from("<I", header, 32)[0] != 0xABCD5432:
        raise ValueError("Firmware is not an ESP application image with an app descriptor")
    return header[48:80].split(b"\0", 1)[0].decode("utf-8")


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

    version = read_current_version()
    firmware_version = read_firmware_version(firmware_src)
    if firmware_version != version:
        raise ValueError(f"Firmware version {firmware_version} does not match release version {version}; rebuild first")

    firmware_dst = os.path.join(OUTPUT_DIR, "firmware.bin")
    spiffs_dst = os.path.join(OUTPUT_DIR, "spiffs.bin")

    shutil.copy(firmware_src, firmware_dst)
    shutil.copy(spiffs_src, spiffs_dst)

    print(f"firmware.bin & spiffs.bin kopiert nach {OUTPUT_DIR}")

    write_manifest(version, firmware_dst, spiffs_dst)
    create_zip_with_timestamp(firmware_dst, spiffs_dst)

    print("OTA-Paket-Build abgeschlossen")

firmware_image = env.File("$BUILD_DIR/${PROGNAME}.bin")  # type: ignore
filesystem_image = env.DataToBin(  # type: ignore
    str(Path("$BUILD_DIR") / "${ESP32_FS_IMAGE_NAME}"),
    "$PROJECT_DATA_DIR",
)
env.NoCache(filesystem_image)  # type: ignore
env.Depends(filesystem_image, firmware_image)  # type: ignore
env.Depends(env.Alias("buildprog"), filesystem_image)  # type: ignore
env.AddPostAction(filesystem_image, package_existing_build)  # type: ignore
