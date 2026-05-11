#!/usr/bin/env bash
# Set up an Ubuntu 24.04 VM for Networking from Scratch.
#
# Run on a fresh Ubuntu 24.04 LTS VM. Do NOT run on your daily-driver machine
# unless you accept that several lessons will load kernel modules and modify
# routing/firewall state.

set -euo pipefail

if [[ "$EUID" -ne 0 ]]; then
    echo "Re-running with sudo..."
    exec sudo -E "$0" "$@"
fi

OS_VERSION="$(. /etc/os-release; echo "${VERSION_ID:-unknown}")"
if [[ "$OS_VERSION" != "24.04" ]]; then
    echo "Warning: this script is tested on Ubuntu 24.04. You're on ${OS_VERSION}."
    echo "Press Ctrl-C to abort, Enter to continue."
    read -r
fi

echo ">> apt update"
apt-get update -y

echo ">> install build toolchain"
apt-get install -y \
    build-essential gcc-13 clang-18 \
    make cmake \
    git curl wget \
    python3.12 python3-pip python3-venv pipx

echo ">> install kernel & networking tooling"
apt-get install -y \
    linux-headers-"$(uname -r)" \
    linux-tools-"$(uname -r)" \
    iproute2 iputils-ping \
    tcpdump tshark wireshark-common \
    iperf3 netcat-openbsd \
    bridge-utils

echo ">> install eBPF / XDP toolchain"
apt-get install -y \
    libbpf-dev bpftool bpftrace \
    linux-libc-dev \
    libelf-dev zlib1g-dev

echo ">> install crypto / TLS deps"
apt-get install -y \
    libsodium-dev libssl-dev

echo ">> install testing / lint deps"
apt-get install -y shellcheck

# Per-user setup
TARGET_USER="${SUDO_USER:-$(whoami)}"
TARGET_HOME="$(getent passwd "$TARGET_USER" | cut -d: -f6)"

echo ">> set up Python venv at .venv (as $TARGET_USER)"
sudo -u "$TARGET_USER" bash -c "
    cd '$TARGET_HOME/networking-from-scratch' 2>/dev/null || cd '$(pwd)'
    python3 -m venv .venv
    .venv/bin/pip install -U pip
    .venv/bin/pip install tox pytest scapy mypy ruff
"

echo ">> done."
echo
echo "Next steps:"
echo "  1. source .venv/bin/activate"
echo "  2. tox -e lint,type    # confirm everything works"
echo "  3. make -C phases/01-bits-and-wires/02-bits-bytes-endianness"
echo
echo "If you plan to do Phase 15 (DDS / robotics) you'll need a PREEMPT_RT"
echo "kernel (Linux 6.13+ has it mainlined). See phases/15-dds-robotics-bonus/README.md."
