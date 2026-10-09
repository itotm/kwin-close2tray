#!/bin/sh
# Build the .deb in a clean Debian 13 (trixie) container.
#   ./packaging/build-deb.sh
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
version=$(sed -n 's/.*"Version": "\(.*\)".*/\1/p' "$root/package/metadata.json")

out="$root/dist"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$out"

cd "$root"
git ls-files -co --exclude-standard | grep -v '^dist/' | tar -T - -czf "$work/source.tar.gz"
cp "$root/packaging/deb-build-in-container.sh" "$work/"

podman run --rm --security-opt label=disable \
    -v "$work":/sources:ro -v "$out":/out \
    -e VERSION="$version-1+deb13" \
    docker.io/library/debian:13 sh /sources/deb-build-in-container.sh

ls -1 "$out"/*.deb
