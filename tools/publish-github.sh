#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ $# -ne 1 || ( "$1" != "--private" && "$1" != "--public" ) ]]; then
  printf 'Usage: %s --private|--public\n' "$0" >&2
  exit 2
fi
python3 tools/check_public.py
[[ "$(gh api user --jq .login)" == 'TTplugs' ]] || { printf 'Authenticate GitHub as TTplugs.\n' >&2; exit 1; }
[[ -z "$(git status --porcelain)" ]] || { printf 'Commit pending changes before publication.\n' >&2; exit 1; }
[[ "$(git branch --show-current)" == main ]] || { printf 'Publication expects main.\n' >&2; exit 1; }
git rev-parse --verify HEAD >/dev/null
if git remote get-url origin >/dev/null 2>&1; then
  printf 'An origin already exists; review it before publication.\n' >&2
  exit 1
fi
gh repo create TTplugs/mpc-melody-maker "$1" \
  --description 'Local Hip Hop MIDI generator for Akai MPC standalone by TTplugs' \
  --source . --remote origin --push
