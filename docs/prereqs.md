# Prerequisites & Setup

## What you need to know before starting

This curriculum assumes you can already write code. It doesn't teach C or Python from scratch — it teaches networking, using C and Python as tools.

### C, intermediate level

You should be comfortable with:

- Pointers, including pointers to structs and pointers to pointers
- `struct`, `union`, bit fields, `__attribute__((packed))`
- `memcpy`, `memmove`, `memset`, `memcmp`
- `malloc`, `free`, and ownership thinking
- Function pointers (for callbacks and dispatch tables)
- The C preprocessor (`#define`, `#ifdef`, `#include`)
- Compiling with `gcc -Wall -Wextra` and reading the warnings
- Reading `man 2` and `man 3` pages

If any of that is unfamiliar, work through [Beej's Guide to C](https://beej.us/guide/bgc/) first. It's free, ~200 pages, and covers everything you need.

### Python 3, intermediate level

You should be comfortable with:

- The difference between `bytes` and `str`
- `struct.pack` and `struct.unpack` for binary data
- Generators and iterators
- `asyncio` basics: `async def`, `await`, `asyncio.run`
- `pytest` for tests, fixtures, and parametrization
- The standard library: `socket`, `selectors`, `ipaddress`, `hashlib`

### CS fundamentals

- Binary, hex, two's complement
- Big-endian vs little-endian
- Basic data structures: queues, stacks, hash tables, ring buffers
- Basic concurrency: threads, mutexes, atomics

### Linux command line

- File system navigation, pipes, redirection
- `ip`, `ss`, `tcpdump`, `dig`, `curl`, `nc`
- Building with `make`
- Editing with `vim`, `nano`, or your editor of choice

If `tcpdump -i any -n port 53` doesn't make sense, spend a few hours with the [tcpdump tutorial](https://danielmiessler.com/p/tcpdump/).

---

## Hardware

You need:

- An x86-64 or ARM64 machine running Linux. Phases 9, 10, 13, 15 specifically need Linux.
- 8 GB RAM minimum. 16 GB+ recommended for the kernel-build phases.
- 30 GB free disk for the kernel sources, eBPF toolchain, and Mininet.

**Don't run the kernel-module or namespace lessons on your daily-driver machine.** Use a VM.

---

## Setting up an Ubuntu 24.04 VM

The fastest path is Multipass:

```bash
# Install Multipass (works on macOS, Linux, Windows)
# https://multipass.run

multipass launch --name nfs --cpus 4 --memory 8G --disk 30G 24.04
multipass shell nfs

# Inside the VM:
git clone https://github.com/TanayK07/networking-from-scratch.git
cd networking-from-scratch
./tools/setup-vm.sh
```

`setup-vm.sh` installs everything: gcc, clang, make, libbpf-dev, linux-headers, tcpdump, wireshark-common, mininet, virtme-ng, libsodium-dev, libssl-dev, Python 3 with pip, and a bunch of utility packages.

---

## Alternative: native Linux

If you're already on Linux and OK with breaking things:

```bash
sudo apt update
sudo apt install -y \
    build-essential gcc clang make pkg-config \
    python3 python3-pip python3-venv \
    libbpf-dev linux-headers-$(uname -r) linux-tools-$(uname -r) \
    libsodium-dev libssl-dev libunity-dev \
    iproute2 iputils-ping tcpdump tshark wireshark-common \
    iperf3 netcat-openbsd dnsutils \
    bpftrace bcc-tools \
    qemu-system-x86 mininet \
    afl++ valgrind

pip install --user pytest pytest-asyncio scapy ruff mypy tox aioquic
```

For the DDS phase (P15), you'll also need:

```bash
sudo apt install -y cyclonedds-dev
# or build from source: https://github.com/eclipse-cyclonedds/cyclonedds
```

---

## Verifying your setup

```bash
cd networking-from-scratch
make verify
```

This runs through every required tool and prints what's missing.

You should see output like:

```
✓ gcc 13.2.0
✓ clang 18.1.0
✓ python3 3.12.3
✓ make 4.3
✓ libbpf headers
✓ linux-headers-6.8.0-31-generic
✓ tcpdump 4.99.4
✓ ip / iproute2
✓ ss
✓ libsodium
✓ libssl3 (3.0.13)
All required tools found.
```

If any line says `✗`, install the missing dependency and re-run.
