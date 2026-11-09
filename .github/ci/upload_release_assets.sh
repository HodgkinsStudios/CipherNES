#!/usr/bin/env bash
set -euo pipefail
# Share tested binaries through a DRAFT GitHub Release, avoiding the Actions artifact-storage quota.
# Only default-branch builds may publish; pull requests still run every build/smoke test.
if [[ "${GITHUB_EVENT_NAME:-}" != "push" || "${GITHUB_REF:-}" != "refs/heads/main" ]]; then
    echo 'Not a main-branch push: skipping draft release upload.'
    exit 0
fi
[[ $# -ge 1 ]] || { echo 'Expected at least one built release file.' >&2; exit 2; }
for asset in "$@"; do [[ -s "$asset" ]] || { echo "Missing/empty release file: $asset" >&2; exit 2; }; done
VERSION="$(tr -d '\r\n[:space:]' < VERSION)"
TAG="v${VERSION}"
[[ -n "${GH_TOKEN:-}" ]] || { echo 'GH_TOKEN missing (contents: write required).' >&2; exit 2; }
[[ -n "${GITHUB_REPOSITORY:-}" && -n "${GITHUB_SHA:-}" ]] || { echo 'GitHub ref context missing.' >&2; exit 2; }
# Concurrency-safe: seven independently tested platform jobs may create the same draft.
for attempt in 1 2 3 4 5 6 7 8 9 10; do
    if gh release view "$TAG" --repo "$GITHUB_REPOSITORY" >/dev/null 2>&1; then break; fi
    gh release create "$TAG" --repo "$GITHUB_REPOSITORY" --target "$GITHUB_SHA" \
        --title "CipherNES ${VERSION}" --notes-file "RELEASE_NOTES_${VERSION}.md" --draft >/dev/null 2>&1 || true
    if gh release view "$TAG" --repo "$GITHUB_REPOSITORY" >/dev/null 2>&1; then break; fi
    sleep 4
done
gh release view "$TAG" --repo "$GITHUB_REPOSITORY" >/dev/null || { echo 'Could not create or access draft release.' >&2; exit 1; }
# Never overwrite v1.0.0 assets with bits from a later commit while its tag is unchanged.
tag_sha="$(gh api "repos/${GITHUB_REPOSITORY}/git/ref/tags/${TAG}" --jq '.object.sha')"
if [[ "$tag_sha" != "$GITHUB_SHA" ]]; then
    echo "Release tag ${TAG} points to ${tag_sha}, not ${GITHUB_SHA}; skipping stale-release upload."
    exit 0
fi
echo "Uploading $# release file(s) to draft ${TAG} at ${GITHUB_SHA}"
gh release upload "$TAG" "$@" --repo "$GITHUB_REPOSITORY" --clobber
echo "Draft release: https://github.com/${GITHUB_REPOSITORY}/releases/tag/${TAG}"
