"""Build the checkout's Qt recipe in CI without changing ordinary Qt cache references."""

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--host-arch")
    args = parser.parse_args()

    if not re.fullmatch(r"6\.\d+\.\d+", args.version):
        parser.error("Expected a Qt 6 release version, such as 6.11.1.")

    run_id = os.environ.get("GITHUB_RUN_ID", "")
    run_attempt = os.environ.get("GITHUB_RUN_ATTEMPT", "")
    if not re.fullmatch(r"\d+", run_id) or not re.fullmatch(r"\d+", run_attempt):
        parser.error("This validation requires GITHUB_RUN_ID and GITHUB_RUN_ATTEMPT from GitHub Actions.")

    conan = shutil.which("conan")
    if conan is None:
        parser.error("Conan is not available in the runner environment.")

    # User/channel distinguishes this recipe from qt/<version> used by ordinary builds.
    channel = f"run-{run_id}-{run_attempt}"
    reference = f"qt/{args.version}@ci/{channel}"
    recipe = Path(__file__).resolve().parent / "recipes" / "qt" / "all"
    command = [conan, "create", str(recipe), f"--version={args.version}",
               "--user=ci", f"--channel={channel}", "-pr:a", args.profile,
               "-s:a", "build_type=Release", f"--build={reference}", "--no-remote"]
    if args.host_arch:
        command += ["-s:h", f"arch={args.host_arch}"]

    print(f"Validating {reference} with profile {args.profile}", flush=True)
    try:
        # Only Qt is built. No global update, cache reset, profile changes or credential copies.
        subprocess.run(command, check=True)
    finally:
        # The exact run-specific reference cannot match ordinary Qt packages or other runs.
        subprocess.run([conan, "remove", reference, "--confirm"], check=True)


if __name__ == "__main__":
    main()
