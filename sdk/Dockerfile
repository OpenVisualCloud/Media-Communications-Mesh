# SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause

# Use the specific version of ubuntu image
FROM ubuntu:22.04@sha256:2b7412e6465c3c7fc5bb21d3e6f1917c167358449fecac8176c6e496e5c1f05f as builder

# Set the maintainer label
LABEL maintainer="qiang.han@intel.com"

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive

# Update and install package dependencies in a single RUN command to reduce layer size
RUN apt-get update && apt-get full-upgrade -y && \
    apt-get install --no-install-recommends -y apt-utils sudo build-essential cmake git ca-certificates curl libbsd-dev && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Create a non-root user
ARG USER=docker
ARG UID=1000
RUN useradd -l -m -s /bin/bash -u ${UID} ${USER}

# Set default environment variables for the user
ARG PREFIX_DIR="/usr/local"
ARG HOME="/home/${USER}"
ENV PATH="${PREFIX_DIR}/bin:$HOME/.local/bin:$HOME/bin:$HOME/usr/bin:$PATH"

# Set the MCM directory
ARG MCM_DIR=/opt/mcm

# Clone the MCM repository and build the application
RUN git clone --depth 1 --branch main https://github.com/OpenVisualCloud/Media-Communications-Mesh.git ${MCM_DIR}
WORKDIR $MCM_DIR/sdk
RUN ./build.sh

## Re-build container for optimised runtime environment using clean Ubuntu release
FROM ubuntu:22.04@sha256:2b7412e6465c3c7fc5bb21d3e6f1917c167358449fecac8176c6e496e5c1f05f

# Set the maintainer label
LABEL maintainer="qiang.han@intel.com"

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive

# Update and install package dependencies in a single RUN command to reduce layer size
RUN apt-get update && apt-get full-upgrade -y && \
    apt-get install --no-install-recommends -y libbsd0 && \
    apt-get install --no-install-recommends -y sleepenh net-tools && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

ARG USER=docker
ARG UID=1000
RUN useradd -l -m -s /bin/bash -u ${UID} ${USER}

# Set default environment variables for the user
ARG PREFIX_DIR="/usr/local"
ARG HOME="/home/${USER}"
ENV PATH="${PREFIX_DIR}/bin:$HOME/.local/bin:$HOME/bin:$HOME/usr/bin:$PATH"

# Set the MCM directory
ARG MCM_DIR=/opt/mcm

# Copy the application build from the builder stage
COPY --from=builder $MCM_DIR/sdk/out/samples /opt/mcm/sdk/out/samples
COPY --from=builder ${PREFIX_DIR} ${PREFIX_DIR}

RUN ldconfig

USER ${USER}

# Set the working directory
WORKDIR /opt/mcm/sdk/out/samples

# Set the default command to run the application
CMD ["./recver_app"]
