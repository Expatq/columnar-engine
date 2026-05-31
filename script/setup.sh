#!/usr/bin/env bash
set -Eeuo pipefail
export DEBIAN_FRONTEND=noninteractive

apt-get update
apt-get install -y --no-install-recommends \
    build-essential ninja-build clang curl ca-certificates gpg

# Install CMake from Kitware's official APT repo (system cmake may be broken/missing).
curl -fsSL https://apt.kitware.com/keys/kitware-archive-latest.asc \
    | gpg --dearmor -o /usr/share/keyrings/kitware-archive-keyring.gpg
. /etc/os-release
echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] \
https://apt.kitware.com/ubuntu/ ${UBUNTU_CODENAME} main" \
    > /etc/apt/sources.list.d/kitware.list
apt-get update
apt-get install -y --no-install-recommends cmake

rm -rf /var/lib/apt/lists/*

# Bazelisk is required to build tcmalloc.
BAZELISK_VERSION="v1.25.0"
ARCH="$(uname -m)"
case "$ARCH" in
    x86_64)  BAZELISK_ARCH="amd64" ;;
    aarch64) BAZELISK_ARCH="arm64" ;;
    *)       echo "Unsupported architecture: $ARCH" >&2; exit 1 ;;
esac

curl -fsSL \
    "https://github.com/bazelbuild/bazelisk/releases/download/${BAZELISK_VERSION}/bazelisk-linux-${BAZELISK_ARCH}" \
    -o /usr/local/bin/bazelisk
chmod +x /usr/local/bin/bazelisk
ln -sf /usr/local/bin/bazelisk /usr/local/bin/bazel
