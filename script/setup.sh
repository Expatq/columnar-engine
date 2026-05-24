#!/usr/bin/env bash
set -Eeuo pipefail
export DEBIAN_FRONTEND=noninteractive

apt-get update
apt-get install -y --no-install-recommends \
    build-essential ninja-build cmake clang curl
rm -rf /var/lib/apt/lists/*

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
