#!/usr/bin/env python3

import argparse
import re
import subprocess
import sys
from pathlib import Path
from packaging.version import Version, InvalidVersion

# Configuration
CONAN_REMOTE = 'conancenter'
CONANFILE = 'conanfile.py'


def extract_dependencies():
    """Extract dependencies from conanfile.py."""
    conanfile_path = Path(CONANFILE)
    if not conanfile_path.exists():
        print(f"Error: {CONANFILE} not found.")
        return []

    content = conanfile_path.read_text()

    # Match requires = (...) or requires = [...]
    match = re.search(
        r'(?:^|\n)\s*requires\s*=\s*(\((?:.*?)\)|\[(?:.*?)\])',
        content,
        re.DOTALL
    )
    if match:
        requires_block = match.group(1)
        deps = re.findall(r'["\']([^"\']+)["\']', requires_block)
        return deps

    # Match requires = "..."
    match = re.search(
        r'(?:^|\n)\s*requires\s*=\s*["\']([^"\']+)["\']',
        content
    )
    if match:
        return [match.group(1)]

    return []


def latest_version_from(dep_name, output):
    """Parse conan search output to find the latest version."""
    versions = []
    # Pattern to match dep_name/version
    # Perl regex: /^\s*\Q$dep_name\E\/([0-9A-Za-z_.+\-]+)/
    pattern = re.compile(
        rf'^\s*{re.escape(dep_name)}/([0-9A-Za-z_.+\-]+)',
        re.MULTILINE
    )

    for match in pattern.finditer(output):
        v_str = match.group(1)
        try:
            versions.append(Version(v_str))
        except InvalidVersion:
            # Fallback for non-PEP 440 versions if necessary,
            # but packaging.version is requested.
            continue

    if not versions:
        return None

    return sorted(versions)[-1]


def check_dependency(entry):
    """Check a single dependency for updates."""
    parts = entry.split('/', 1)
    if len(parts) != 2:
        print(f"⚠ Unable to parse dependency entry: {entry}")
        return

    dep_name, rest = parts
    current_version_str = rest.split('@', 1)[0]

    try:
        current_version = Version(current_version_str)
    except InvalidVersion:
        print(f"⚠ Unable to determine current version for {entry}")
        return

    try:
        # Run conan search
        result = subprocess.run(
            ['conan', 'search', dep_name, '-r', CONAN_REMOTE],
            capture_output=True,
            text=True,
            check=False
        )
        output = result.stdout + result.stderr
    except Exception as e:
        print(f"⚠ Error running conan search for {dep_name}: {e}")
        return

    if result.returncode != 0:
        print(f"⚠ {dep_name} search failed (current {current_version})")
        print("\n--- Conan Output ---")
        print(output)
        print("--- End Conan Output ---")
        return

    latest_version = latest_version_from(dep_name, output)

    if latest_version is None:
        print(
            f"⚠ {dep_name} unable to determine latest version "
            f"(current {current_version})"
        )
        return

    if current_version == latest_version:
        print(f"✓ {dep_name} is up to date (version {current_version})")
    elif current_version < latest_version:
        print(
            f"⚠ {dep_name} update available "
            f"({current_version} → {latest_version})"
        )
    else:
        # current_version > latest_version
        print(
            f"✓ {dep_name} is up to date (version {current_version} "
            f"is newer than remote {latest_version})"
        )


def main():
    parser = argparse.ArgumentParser(
        description="Check for updated Conan dependencies"
    )
    parser.parse_args()

    print("Checking for Conan dependency updates...")

    dependencies = extract_dependencies()

    if not dependencies:
        print(f"No dependencies found in {CONANFILE}")
        sys.exit(0)

    for dep in dependencies:
        check_dependency(dep)


if __name__ == "__main__":
    main()
