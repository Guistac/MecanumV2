# PlatformIO pre-build script: FW_VERSION = the git commit of the build, for the remote's About page
import subprocess

Import("env")

try:
    version = subprocess.check_output(
        ["git", "describe", "--always", "--dirty", "--abbrev=7"],
        cwd=env.subst("$PROJECT_DIR")).decode().strip()
except Exception:
    version = "unknown"

env.Append(CPPDEFINES=[("FW_VERSION", env.StringifyMacro(version))])
