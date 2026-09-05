#!/usr/bin/env bash
# Renders the whole specification (README.md as the index, plus every docs/*.md)
# and publishes it to the public MinIO artifacts bucket under a dedicated folder,
# so the spec is readable from a phone at the URL below.
#
# Invoked automatically by the PostToolUse hook in .claude/settings.json whenever
# Claude edits README.md or a file in docs/; safe to run by hand at any time.
#
# The published objects are world-readable by anyone with the link.

set -euo pipefail

PREFIX="${DOCS_PUBLISH_PREFIX:-esp32-sprinkler}"
ALIAS="${DOCS_PUBLISH_ALIAS:-artifacts-upload}"
BUCKET="${DOCS_PUBLISH_BUCKET:-artifacts}"
BASE_URL="${DOCS_PUBLISH_BASE_URL:-https://artifacts.cbx95.kitesize.fr}"
INDEX_URL="${BASE_URL}/${PREFIX}/index.html"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

# `mc` is commonly a shell alias to ~/aistor-binaries/mc, which does not survive
# into a non-interactive hook shell, so resolve the binary explicitly.
MC="$(command -v mc || true)"
[ -x "$MC" ] || MC="$HOME/aistor-binaries/mc"
if [ ! -x "$MC" ]; then
  echo "publish-docs: mc not found (looked on PATH and in ~/aistor-binaries)" >&2
  exit 1
fi

NODE="$(command -v node || true)"
if [ ! -x "$NODE" ]; then
  # Hooks inherit a bare PATH that misses nvm's shims.
  NODE="$(ls -d "$HOME"/.nvm/versions/node/*/bin/node 2>/dev/null | sort -V | tail -1 || true)"
fi
if [ ! -x "$NODE" ]; then
  echo "publish-docs: node not found (needed to render the markdown)" >&2
  exit 1
fi

stage="$(mktemp -d -t docs-publish)"
trap 'rm -rf "$stage"' EXIT

# The whole set is rendered and uploaded every time: it is 20-odd small pages, and
# a partial upload would leave cross-links pointing at stale pages.
sources=(README.md docs/*.md)
"$NODE" scripts/render-docs.mjs "$stage" "${sources[@]}"

"$MC" cp --quiet --recursive --attr "Content-Type=text/html;charset=utf-8" \
  "$stage/" "${ALIAS}/${BUCKET}/${PREFIX}/" >/dev/null

# Check the index and one nested page, so a broken docs/ prefix does not pass.
for url in "$INDEX_URL" "${BASE_URL}/${PREFIX}/docs/01-overview.html"; do
  code="$(curl -sS -o /dev/null -w '%{http_code}' "$url")"
  if [ "$code" != "200" ]; then
    echo "publish-docs: uploaded but ${url} returned HTTP ${code}" >&2
    exit 1
  fi
done

echo "publish-docs: ${#sources[@]} pages at ${INDEX_URL} (HTTP 200)"
