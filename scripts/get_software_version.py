from pathlib import Path

Import("env")  # type: ignore

def read_version():
    project_dir = Path(env.subst("$PROJECT_DIR"))  # type: ignore
    version_file = project_dir / "bin" / "version.txt"

    if not version_file.exists():
        raise FileNotFoundError(f"Missing version file: {version_file}")

    return version_file.read_text(encoding="utf-8").strip()

env.Append(  # type: ignore
    BUILD_FLAGS=[
        '-D SOFTWARE_VERSION=\\"' + read_version() + '\\"'
    ]
)