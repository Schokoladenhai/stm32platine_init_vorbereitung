# Schlanke Entwicklungsumgebung: nur Toolchain, Build, Flashen, Debuggen.
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib \
        cmake ninja-build git \
        openocd dfu-util gdb-multiarch usbutils \
        clang-format sudo ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Normaler Benutzer mit UID 1000 (wie der erste Benutzer in WSL), sudo ohne Passwort
# für openocd und dfu-util, weil wir keine udev-Regeln verwenden.
RUN userdel -r ubuntu 2>/dev/null || true \
    && useradd -m -s /bin/bash -u 1000 -G plugdev,dialout dev \
    && echo 'dev ALL=(ALL) NOPASSWD:ALL' > /etc/sudoers.d/dev

# build.sh und flash.sh von überall aufrufbar
ENV PATH="/workspace/tools:${PATH}"

USER dev
WORKDIR /workspace
