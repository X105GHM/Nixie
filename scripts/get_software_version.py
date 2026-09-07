from pathlib import Path
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

env.Append(  # type: ignore
    BUILD_FLAGS=[
        f'-DSOFTWARE_VERSION=\\"{version}\\"'
    ]
)

print(f"SOFTWARE_VERSION set to: {version}")
