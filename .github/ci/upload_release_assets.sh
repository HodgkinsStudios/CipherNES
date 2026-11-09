#!/usr/bin/env bash
set -euo pipefail
# Publish checked binaries to a GitHub Release draft, without Actions artifact storage.
if [[ "${GITHUB_EVENT_NAME:-}" != 'push' || "${GITHUB_REF:-}" != 'refs/heads/main' ]]; then
    echo 'Not a main-branch push: skipping draft release upload.'
    exit 0
fi
[[ $# -ge 1 ]] || { echo 'No assets specified.' >&2; exit 2; }
for asset in "$@"; do [[ -s "$asset" ]] || { echo "Missing/empty asset: $asset" >&2; exit 2; }; done
VERSION="$(tr -d '\r\n[:space:]' < VERSION)"
TAG="v${VERSION}"
[[ -n "${GH_TOKEN:-}" && -n "${GITHUB_SHA:-}" && -n "${GITHUB_REPOSITORY:-}" ]] || { echo 'GitHub release credentials/context missing.' >&2; exit 2; }
# Before creating anything, verify this build is the current release source.
main_sha="$(gh api "repos/${GITHUB_REPOSITORY}/branches/main" --jq '.commit.sha')"
if [[ "$main_sha" != "$GITHUB_SHA" ]]; then
    echo "Superseded build $GITHUB_SHA; current main is $main_sha. Skipping."
    exit 0
fi
# An actual lightweight tag ref MUST exist before creating the draft. Otherwise
# GitHub may assign an untagged-* placeholder when the draft is modified.
if ! gh api "repos/${GITHUB_REPOSITORY}/git/ref/tags/${TAG}" >/dev/null 2>&1; then
    gh api -X POST "repos/${GITHUB_REPOSITORY}/git/refs" \
        -f "ref=refs/tags/${TAG}" -f "sha=${GITHUB_SHA}" >/dev/null 2>&1 || true
fi
tag_sha="$(gh api "repos/${GITHUB_REPOSITORY}/git/ref/tags/${TAG}" --jq '.object.sha')"
if [[ "$tag_sha" != "$GITHUB_SHA" ]]; then
    echo "Release ${TAG} is already pinned at ${tag_sha}; this build is ${GITHUB_SHA}. Skipping."
    exit 0
fi
# Multiple OS jobs race to create the SAME release. All subsequent jobs reuse it.
for attempt in 1 2 3 4 5 6 7 8 9 10; do
    if gh release view "$TAG" --repo "$GITHUB_REPOSITORY" >/dev/null 2>&1; then break; fi
    gh release create "$TAG" --repo "$GITHUB_REPOSITORY" --verify-tag \
        --title "CipherNES ${VERSION}" --notes-file "RELEASE_NOTES_${VERSION}.md" --draft >/dev/null 2>&1 || true
    sleep 3
done
gh release view "$TAG" --repo "$GITHUB_REPOSITORY" >/dev/null || { echo "Draft ${TAG} unavailable." >&2; exit 1; }
if [[ "$(gh release view "$TAG" --repo "$GITHUB_REPOSITORY" --json isDraft --jq '.isDraft')" != 'true' ]]; then
    echo "Release ${TAG} is already published; leaving assets unchanged."
    exit 0
fi
echo "Uploading $# asset(s) from ${GITHUB_SHA} into draft ${TAG}."
gh release upload "$TAG" "$@" --repo "$GITHUB_REPOSITORY" --clobber
echo "Draft release: https://github.com/${GITHUB_REPOSITORY}/releases/tag/${TAG}"
