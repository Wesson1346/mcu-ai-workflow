#!/usr/bin/env bash
# install.sh - Install mcu-project-orchestrator skill (macOS / Linux)
#
# Usage:
#   ./install.sh           install (skip if already installed)
#   ./install.sh --force   overwrite existing installation
#
# What it does:
#   1. Copies orchestrator/ into  ~/.workbuddy/skills/mcu-project-orchestrator
#   2. Replaces {{AI_SKILL_ROOT}} with the real absolute path of this repo's AI-Skill folder

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKILL_ROOT="$REPO_ROOT/AI-Skill"
SOURCE="$REPO_ROOT/orchestrator"
TARGET="$HOME/.workbuddy/skills/mcu-project-orchestrator"
FORCE=0

for arg in "$@"; do
  case "$arg" in
    --force|-f) FORCE=1 ;;
    *) echo "Unknown argument: $arg"; exit 1 ;;
  esac
done

if [ ! -d "$SKILL_ROOT" ]; then
  echo "ERROR: AI-Skill folder not found at: $SKILL_ROOT" >&2
  echo "Run this script from the repository root." >&2
  exit 1
fi
if [ ! -d "$SOURCE" ]; then
  echo "ERROR: orchestrator folder not found at: $SOURCE" >&2
  exit 1
fi

if [ -d "$TARGET" ]; then
  if [ "$FORCE" -ne 1 ]; then
    echo "Already installed at: $TARGET"
    echo "Re-run with --force to overwrite."
    exit 0
  fi
  echo "Removing existing installation..."
  rm -rf "$TARGET"
fi

mkdir -p "$TARGET"
cp -R "$SOURCE/." "$TARGET/"

PATCHED=0
while IFS= read -r -d '' f; do
  if grep -q '{{AI_SKILL_ROOT}}' "$f" 2>/dev/null; then
    # use | as delimiter because paths may contain /
    sed -i.bak "s|{{AI_SKILL_ROOT}}|$SKILL_ROOT|g" "$f"
    rm -f "$f.bak"
    PATCHED=$((PATCHED + 1))
  fi
done < <(find "$TARGET" -name '*.md' -print0)

echo ""
echo "Installed : $TARGET"
echo "Skill root: $SKILL_ROOT"
echo "Patched   : $PATCHED file(s)"
echo ""
echo "Next step:"
echo "  Open any project folder in your AI IDE and say:"
echo "    I want to build a temperature-controlled fan"
echo ""
echo "  The orchestrator will first show a plan card and WAIT for your confirmation."
