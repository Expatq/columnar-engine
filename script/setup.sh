#!/usr/bin/env bash
set -Eeuo pipefail
export DEBIAN_FRONTEND=noninteractive

apt-get update
apt-get install -y --no-install-recommends \
    build-essential ninja-build curl ca-certificates gpg wget

# Install clang-20 + llvm-20 from llvm.org APT repo (default clang-18 in noble cannot build google/tcmalloc).
wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor -o /usr/share/keyrings/llvm.gpg
. /etc/os-release
echo "deb [signed-by=/usr/share/keyrings/llvm.gpg] http://apt.llvm.org/${UBUNTU_CODENAME}/ llvm-toolchain-${UBUNTU_CODENAME}-20 main" \
    > /etc/apt/sources.list.d/llvm-20.list

# Install CMake from Kitware's official APT repo (system cmake may be broken/missing).
curl -fsSL https://apt.kitware.com/keys/kitware-archive-latest.asc \
    | gpg --dearmor -o /usr/share/keyrings/kitware-archive-keyring.gpg
echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ ${UBUNTU_CODENAME} main" \
    > /etc/apt/sources.list.d/kitware.list

apt-get update
apt-get install -y --no-install-recommends clang-20 lld-20 llvm-20 cmake

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
