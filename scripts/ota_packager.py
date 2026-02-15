#!/usr/bin/env python3
import os
import shutil
import zipfile
import subprocess
import hashlib
import json
from datetime import datetime

# Pfad zum Projekt-Root (eine Ebene über diesem Skript)
PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUTPUT_DIR = os.path.join(PROJECT_DIR, 'bin')
VERSION_FILE = os.path.join(OUTPUT_DIR, 'version.txt')

# Sicherstellen, dass das bin-Verzeichnis existiert
os.makedirs(OUTPUT_DIR, exist_ok=True)


def sha256sum(filepath):
    h = hashlib.sha256()
    with open(filepath, 'rb') as f:
        for chunk in iter(lambda: f.read(4096), b''):
            h.update(chunk)
    return h.hexdigest()


def get_next_version():
    base = 'Nixie_V.6.6.'
    if os.path.exists(VERSION_FILE):
        with open(VERSION_FILE, 'r') as vf:
            last = vf.read().strip()
            try:
                n = int(last.split('.')[-1]) + 1
            except ValueError:
                n = 0
    else:
        n = 0
    version = f"{base}{n}"
    with open(VERSION_FILE, 'w') as vf:
        vf.write(version)
    return version


def write_manifest(version, firmware_path, spiffs_path):
    now = datetime.now()
    manifest = {
        'version': version,
        'timestamp': now.strftime('%Y-%m-%d %H:%M:%S'),
        'files': {
            'firmware.bin': sha256sum(firmware_path),
            'spiffs.bin': sha256sum(spiffs_path)
        }
    }
    manifest_path = os.path.join(OUTPUT_DIR, 'manifest.json')
    with open(manifest_path, 'w') as mf:
        json.dump(manifest, mf, indent=2)
    print(f"📝 Manifest geschrieben nach {manifest_path}")


def create_zip_with_timestamp(firmware_path, spiffs_path):
    timestamp = datetime.now().strftime('%Y%m%d_%H%M')
    zip_name = f"ota_package_{timestamp}.zip"
    zip_path = os.path.join(OUTPUT_DIR, zip_name)
    with zipfile.ZipFile(zip_path, 'w') as zipf:
        zipf.write(firmware_path, arcname='firmware.bin')
        zipf.write(spiffs_path, arcname='spiffs.bin')
    print(f"📦 OTA-Archiv erstellt: {zip_name}")


def build_package():
    print('📦 Starte Build von Firmware und SPIFFS...')
    # Firmware bauen
    subprocess.check_call(['pio', 'run'])
    # SPIFFS bauen
    subprocess.check_call(['pio', 'run', '--target', 'buildfs'])

    # Build-Verzeichnis für Env
    build_dir = os.path.join(PROJECT_DIR, '.pio', 'build', 'esp32-s3-wroom-1-n16r8')
    # Liste binärer Dateien
    bins = [f for f in os.listdir(build_dir) if f.endswith('.bin')]
    # Entferne das SPIFFS-Image
    if 'spiffs.bin' in bins:
        bins.remove('spiffs.bin')
    if not bins:
        print('❌ Keine Firmware-Datei (.bin) gefunden im Build-Verzeichnis')
        return
    # Nimm die erste Firmware-Datei
    firmware_name = bins[0]
    firmware_src = os.path.join(build_dir, firmware_name)
    spiffs_src = os.path.join(build_dir, 'spiffs.bin')

    firmware_dst = os.path.join(OUTPUT_DIR, 'firmware.bin')
    spiffs_dst = os.path.join(OUTPUT_DIR, 'spiffs.bin')

    # Kopieren
    if not os.path.exists(firmware_src):
        print(f"❌ Firmware nicht gefunden: {firmware_src}")
        return
    if not os.path.exists(spiffs_src):
        print(f"❌ SPIFFS nicht gefunden: {spiffs_src}")
        return
    shutil.copy(firmware_src, firmware_dst)
    shutil.copy(spiffs_src, spiffs_dst)
    print(f"✅ firmware.bin & spiffs.bin kopiert nach {OUTPUT_DIR}")

    # Version & Manifest
    version = get_next_version()
    write_manifest(version, firmware_dst, spiffs_dst)

    # Archiv erzeugen
    create_zip_with_timestamp(firmware_dst, spiffs_dst)
    print('✅ OTA-Paket-Build abgeschlossen')


if __name__ == '__main__':
    from datetime import datetime
    build_package()
