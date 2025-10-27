#!/bin/sh

check_dependency() {
    local dep_name="$1"
    local current_version=$(grep -E "^${dep_name}/" conanfile.txt | cut -d'/' -f2)
    local output=$(conan search "$dep_name" -r conancenter 2>/dev/null)
    local latest_version=$(echo "$output" | grep -E "^\s+${dep_name}/[0-9]" | tail -1 | sed "s/^\s*${dep_name}\///")

    if [ "$current_version" = "$latest_version" ]; then
        echo "✓ $dep_name is up to date (version $current_version)"
    else
        echo "⚠ $dep_name update available ($current_version → $latest_version)"
    fi
}

echo "Checking for Conan dependency updates..."
check_dependency "boost"
check_dependency "catch2"
