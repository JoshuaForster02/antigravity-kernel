# Dockerfile for OS Development
# Ubuntu ships no i686-elf cross compiler package, so the Makefile falls back
# to the host gcc in 32-bit mode (gcc-multilib). QEMU is included for `make test`.
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    nasm \
    gcc \
    gcc-multilib \
    binutils \
    make \
    python3 \
    qemu-system-x86 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /os
