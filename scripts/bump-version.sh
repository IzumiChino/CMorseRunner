#!/usr/bin/env bash
# Usage: scripts/bump-version.sh [patch|minor|major]
# Bumps the version in CMakeLists.txt, creates a commit and a git tag.
# Run from the repository root.
set -euo pipefail

BUMP=${1:-patch}
CMAKELISTS="CMakeLists.txt"

# Read current version (supports "1.68" or "1.68.0" style)
CURRENT=$(grep -oP '(?<=project\(MorseRunner VERSION )[0-9]+\.[0-9]+(\.[0-9]+)?' "$CMAKELISTS")
if [ -z "$CURRENT" ]; then
    echo "error: could not find version in $CMAKELISTS" >&2
    exit 1
fi

# Normalise to three components
IFS='.' read -r MAJOR MINOR PATCH <<< "${CURRENT}.0"
PATCH=${PATCH:-0}

case "$BUMP" in
    major) MAJOR=$((MAJOR + 1)); MINOR=0; PATCH=0 ;;
    minor) MINOR=$((MINOR + 1)); PATCH=0 ;;
    patch) PATCH=$((PATCH + 1)) ;;
    *)     echo "Usage: $0 [patch|minor|major]" >&2; exit 1 ;;
esac

NEW="${MAJOR}.${MINOR}.${PATCH}"

# Update CMakeLists.txt
sed -i "s/project(MorseRunner VERSION ${CURRENT}/project(MorseRunner VERSION ${NEW}/" "$CMAKELISTS"

echo "Version bumped: ${CURRENT} -> ${NEW}"

git add "$CMAKELISTS"
git commit -m "chore: bump version to ${NEW}"
git tag -a "v${NEW}" -m "Release v${NEW}"

echo ""
echo "Tag v${NEW} created locally. To publish the release:"
echo "  git push origin main v${NEW}"
