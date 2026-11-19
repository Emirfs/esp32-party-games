# PlatformIO pre-script: builds web/ so the firmware never embeds a stale UI.
import os
import shutil
import subprocess

Import("env")  # noqa: F821 (provided by PlatformIO)

web_dir = os.path.normpath(os.path.join(env["PROJECT_DIR"], "..", "web"))  # noqa: F821
dist = [os.path.join(web_dir, "dist", name) for name in ("index.html.gz", "portal.html.gz")]
npm = shutil.which("npm")

if npm is None:
    if all(os.path.isfile(p) for p in dist):
        print("build_web: npm not found, embedding the existing web/dist")
    else:
        raise SystemExit("build_web: npm not found and web/dist is missing; install Node.js 20+")
else:
    if not os.path.isdir(os.path.join(web_dir, "node_modules")):
        subprocess.run([npm, "ci"], cwd=web_dir, check=True)
    subprocess.run([npm, "run", "build"], cwd=web_dir, check=True)
