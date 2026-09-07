"""Check version stability and IDF cache invalidation before a release build."""

import json
from pathlib import Path
import runpy
import tempfile

ROOT = Path(__file__).resolve().parents[1]


class BuildEnv:
    def __init__(self, project, build):
        self.paths = {"$PROJECT_DIR": str(project), "$BUILD_DIR": str(build)}
        self.flags = []

    def subst(self, value):
        return self.paths[value]

    def Append(self, BUILD_FLAGS):
        self.flags.extend(BUILD_FLAGS)


def main():
    with tempfile.TemporaryDirectory(prefix="nixie-version-test-") as directory:
        project = Path(directory)
        build = project / "build"
        build.mkdir()
        (project / "bin").mkdir()
        version_file = project / "bin/version.txt"
        version_file.write_text("Nixie_V.6.7.1\n", encoding="utf-8")
        original = version_file.read_bytes()
        description = build / "project_description.json"
        cache = build / "CMakeCache.txt"
        env = BuildEnv(project, build)

        def run_script():
            runpy.run_path(str(ROOT / "scripts/get_software_version.py"),
                           init_globals={"env": env, "Import": lambda _: None})
            assert version_file.read_bytes() == original, "build changed release version"

        description.write_text(json.dumps({"project_version": "Nixie_V.6.7.0"}), encoding="utf-8")
        cache.write_text("old cache", encoding="utf-8")
        run_script()
        assert not cache.exists(), "stale component defines were retained"

        description.write_text(json.dumps({"project_version": "Nixie_V.6.7.1"}), encoding="utf-8")
        cache.write_text("current cache", encoding="utf-8")
        run_script()
        assert cache.read_text(encoding="utf-8") == "current cache", "unchanged version caused reconfiguration"
        assert all("Nixie_V.6.7.1" in flag for flag in env.flags)

    print("PASS OTA release version: stable version file and correct IDF cache invalidation")


if __name__ == "__main__":
    main()
