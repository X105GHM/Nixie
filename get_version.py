Import("env") # type: ignore

def read_version():
    with open("bin/version.txt") as f:
        return f.read().strip()

env.Append( # type: ignore
    BUILD_FLAGS=["-D SOFTWARE_VERSION=\\\"" + read_version() + "\\\""]
)
