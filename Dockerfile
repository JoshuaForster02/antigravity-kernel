# Dockerfile for OS Development
FROM ubuntu:latest

RUN apt-get update && apt-get install -y \
    nasm \
    gcc-i686-elf \
    binutils-i686-elf \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /os
