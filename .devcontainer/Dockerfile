FROM mcr.microsoft.com/devcontainers/cpp:ubuntu-22.04

ARG DEBIAN_FRONTEND=noninteractive
ARG ZEPHYR_SDK_VERSION=0.17.4

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bash-completion \
        ca-certificates \
        ccache \
        cmake \
        curl \
        device-tree-compiler \
        dfu-util \
        file \
        flex \
        bison \
        gperf \
        gcc-multilib \
        g++-multilib \
        git \
        git-lfs \
        iproute2 \
        less \
        libmagic1 \
        libncurses5 \
        libsdl2-dev \
        libusb-1.0-0 \
        locales \
        make \
        nano \
        ninja-build \
        picocom \
        python3-dev \
        python3-pip \
        python3-setuptools \
        python3-tk \
        python3-venv \
        python3-wheel \
        sudo \
        udev \
        usbutils \
        wget \
        xz-utils \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m pip install --no-cache-dir -U \
        pip \
        setuptools \
        wheel

RUN python3 -m pip install --no-cache-dir -U \
        west \
        esptool \
        pyelftools

RUN mkdir -p /opt/toolchains \
    && wget -q https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64.tar.xz -O /tmp/zephyr-sdk.tar.xz \
    && tar -xf /tmp/zephyr-sdk.tar.xz -C /opt/toolchains \
    && /opt/toolchains/zephyr-sdk-${ZEPHYR_SDK_VERSION}/setup.sh -t all -h -c \
    && ln -s /opt/toolchains/zephyr-sdk-${ZEPHYR_SDK_VERSION} /opt/toolchains/zephyr-sdk \
    && rm -f /tmp/zephyr-sdk.tar.xz

ENV ZEPHYR_TOOLCHAIN_VARIANT=zephyr
ENV ZEPHYR_SDK_INSTALL_DIR=/opt/toolchains/zephyr-sdk

WORKDIR /workspaces/akira-workspace/AkiraOS
