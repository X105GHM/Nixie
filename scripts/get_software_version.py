from pathlib import Path
import json
import re

Import("env")  # type: ignore


def get_version_file():
    project_dir = Path(env.subst("$PROJECT_DIR"))  # type: ignore
    return project_dir / "bin" / "version.txt"


def read_version():
    version_file = get_version_file()
    version = version_file.read_text(encoding="utf-8").strip()
    if not re.fullmatch(r"Nixie_V\.\d+\.\d+\.\d+", version) or len(version) > 31:
        raise ValueError(f"Invalid firmware version: {version}")
    return version


# The release version is explicit: builds, buildfs and IDE configuration must
# not change the version advertised by an already generated OTA package.
version = read_version()

# PlatformIO reads CMake's code model before Ninja notices extra configure
# dependencies. Invalidate the generated cache on a version change so component
# defines (especially esp_app_desc.c's PROJECT_VER) are current in this build.
build_dir = Path(env.subst("$BUILD_DIR"))  # type: ignore
try:
    description = json.loads((build_dir / "project_description.json").read_text(encoding="utf-8"))
except (FileNotFoundError, json.JSONDecodeError):
    description = {}
if description.get("project_version") != version:
    (build_dir / "CMakeCache.txt").unlink(missing_ok=True)

env.Append(  # type: ignore
    BUILD_FLAGS=[
        f'-DSOFTWARE_VERSION=\\"{version}\\"'
    ]
)

print(f"SOFTWARE_VERSION set to: {version}")
