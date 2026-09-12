#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/deps"

clone_at() {
  local url="$1" dest="$2" commit="$3"
  if [ ! -d "$dest/.git" ]; then
    git clone --filter=blob:none "$url" "$dest"
  fi
  git -C "$dest" fetch --depth 1 origin "$commit"
  git -C "$dest" checkout --detach "$commit"
}

clone_at https://github.com/RusJJ/AndroidModLoader.git \
  "$ROOT/deps/AndroidModLoader" 5588ed242b8e49fc1def572aef89c6c9e745aa34
clone_at https://github.com/AndroidModLoader/aml-psdk.git \
  "$ROOT/deps/aml-psdk" 40489f4bb22df2339d47c5bd47b12d60704734ec

echo "Pinned AML + aml-psdk dependencies ready."
