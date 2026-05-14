import sys
import os
import shutil
from SCons.Script import AlwaysBuild  # type: ignore

Import("env")  # type: ignore

global python_exe
python_exe = sys.executable
scripts_dir = os.path.join(env['PROJECT_DIR'], 'scripts')   # type: ignore
version_script       = os.path.join(scripts_dir, 'get_software_version.py')
ota_packager_script = os.path.join(scripts_dir, 'ota_packager.py')

env_package_ota = env.AddCustomTarget(   # type: ignore
    name        = 'package_ota',
    dependencies= [],
    actions     = [f"{python_exe} {ota_packager_script}"],
    title       = '📦 Baue OTA-Paket'
)
AlwaysBuild(env_package_ota)

alias_buildprog = env.Alias("buildprog")   # type: ignore
alias_buildfs   = env.Alias("buildfs")   # type: ignore

def build_bins(source, target, env):
    project_dir = env['PROJECT_DIR']
    build_dir = env.subst("$BUILD_DIR")
    progname = env.subst("$PROGNAME")
    firmware_src = os.path.join(build_dir, f"{progname}.bin")
    spiffs_src = os.path.join(build_dir, "spiffs.bin")
    out_dir = os.path.join(project_dir, "bin")
    os.makedirs(out_dir, exist_ok=True)
    # Prüfen, ob Dateien existieren
    if not os.path.exists(firmware_src):
        print(f"❌ {firmware_src} nicht gefunden")
        return
    if not os.path.exists(spiffs_src):
        print(f"❌ {spiffs_src} nicht gefunden")
        return
    shutil.copy(firmware_src, os.path.join(out_dir, "firmware.bin"))
    shutil.copy(spiffs_src, os.path.join(out_dir, "spiffs.bin"))
    print(f"🔧 Kopiert firmware.bin und spiffs.bin nach {out_dir}")

env_build_bins = env.AddCustomTarget(   # type: ignore
    name        = 'build_bins',
    dependencies= [alias_buildprog, alias_buildfs],
    actions     = [build_bins],
    title       = '🔧 Erzeuge beide BINs und kopiere sie nach bin/'
)
AlwaysBuild(env_build_bins)
