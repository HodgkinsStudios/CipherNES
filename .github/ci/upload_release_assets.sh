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
# Draft releases need not have a Git tag ref until publication.
# Ignore builds that were superseded by another push while this workflow ran.
main_sha="$(gh api "repos/${GITHUB_REPOSITORY}/branches/main" --jq '.commit.sha')"
if [[ "$main_sha" != "$GITHUB_SHA" ]]; then
    echo "Superseded workflow SHA $GITHUB_SHA (main is $main_sha); skipping upload."
    exit 0
fi
target_sha="$(gh release view "$TAG" --repo "$GITHUB_REPOSITORY" --json targetCommitish --jq '.targetCommitish')"
if [[ "$target_sha" != "$GITHUB_SHA" ]]; then
    draft="$(gh release view "$TAG" --repo "$GITHUB_REPOSITORY" --json isDraft --jq '.isDraft')"
    if [[ "$draft" != "true" ]]; then
        echo "Published release $TAG is immutable; not replacing assets from $target_sha"
        exit 0
    fi
    id="$(gh release view "$TAG" --repo "$GITHUB_REPOSITORY" --json databaseId --jq '.databaseId')"
    gh api -X PATCH "repos/${GITHUB_REPOSITORY}/releases/$id" -f target_commitish="$GITHUB_SHA" >/dev/null
    echo "Updated draft release target from $target_sha to $GITHUB_SHA"
fi
echo "Uploading $# release file(s) to draft ${TAG} at ${GITHUB_SHA}"
gh release upload "$TAG" "$@" --repo "$GITHUB_REPOSITORY" --clobber
echo "Draft release: https://github.com/${GITHUB_REPOSITORY}/releases/tag/${TAG}"
