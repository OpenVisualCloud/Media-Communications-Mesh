# syntax=docker/dockerfile:1

# SPDX-FileCopyrightText: Copyright (c) 2023-2024 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause

FROM ubuntu:22.04@sha256:a6d2b38300ce017add71440577d5b0a90460d0e57fd7aec21dd0d1b0761bbfb2 as builder

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Warsaw

ARG MCM_DIR=/opt/mcm
ARG PREFIX_DIR="/usr/local"

SHELL ["/bin/bash", "-exc"]
# To be removed after fixes are merged: sudo
RUN apt-get update && \
    apt-get full-upgrade -y && \
    apt-get install --no-install-recommends -y \
        sudo \
        apt-utils build-essential make cmake git ca-certificates pkg-config nasm libbsd-dev && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY . ${MCM_DIR}
WORKDIR ${MCM_DIR}/sdk
RUN ./build.sh

WORKDIR ${MCM_DIR}/ffmpeg-plugin
RUN ./clone-and-patch-ffmpeg.sh && \
    ./configure-ffmpeg.sh && \
    ./build-ffmpeg.sh

FROM ubuntu:22.04@sha256:a6d2b38300ce017add71440577d5b0a90460d0e57fd7aec21dd0d1b0761bbfb2

LABEL maintainer="milosz.linkiewicz@intel.com"
LABEL org.opencontainers.image.title="Intel® MCM FFmpeg" \
      org.opencontainers.image.description="Intel® MCM FFmpeg with MCM-Muxer and MCM-Demuxer Ubuntu 22.04 Docker Release Image" \
      org.opencontainers.image.version="1.0.0" \
      org.opencontainers.image.vendor="Intel"

ARG HOME="/home/${USER}"
ARG MCM_DIR=/opt/mcm
ARG PREFIX_DIR="/usr/local"

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Warsaw
ENV PATH="${PREFIX_DIR}/bin:$HOME/.local/bin:$HOME/bin:$HOME/usr/bin:$PATH"
ENV LD_LIBRARY_PATH="/usr/local/lib:/usr/local/lib/x86_64-linux-gnu"

SHELL ["/bin/bash", "-exc"]
RUN apt-get update && \
    apt-get full-upgrade -y && \
    apt-get install --no-install-recommends -y \
        libbsd0 vim net-tools ca-certificates sudo && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* && \
    groupadd -g 2110 vfio && \
    useradd -m -s /bin/bash -G vfio -u 1002 mcm && \
    usermod -aG sudo mcm

COPY --chown=mcm --from=builder ${MCM_DIR}/sdk/out/samples /opt/mcm/
COPY --chown=mcm --from=builder ${PREFIX_DIR} ${PREFIX_DIR}

RUN ldconfig

USER mcm
WORKDIR /opt/mcm/

CMD ["ffmpeg"]