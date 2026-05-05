from pathlib import Path
import re

Import("env")  # type: ignore


def get_version_file():
    project_dir = Path(env.subst("$PROJECT_DIR"))  # type: ignore
    return project_dir / "bin" / "version.txt"


def bump_version(version: str) -> str:
    version = version.strip()

    match = re.match(r"^(.*\.)(\d+)$", version)
    if not match:
        raise ValueError(f"Invalid version format: {version}")

    prefix = match.group(1)
    patch = int(match.group(2))

    return f"{prefix}{patch + 1}"


def read_and_increment_version():
    version_file = get_version_file()
    version_file.parent.mkdir(parents=True, exist_ok=True)

    if not version_file.exists():
        version = "Nixie_V.6.6.0"
    else:
        version = version_file.read_text(encoding="utf-8").strip()

    new_version = bump_version(version)

    version_file.write_text(new_version, encoding="utf-8")

    return new_version


version = read_and_increment_version()

env.Append(  # type: ignore
    BUILD_FLAGS=[
        f'-DSOFTWARE_VERSION=\\"{version}\\"'
    ]
)

print(f"SOFTWARE_VERSION set to: {version}")