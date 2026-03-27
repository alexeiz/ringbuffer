FROM mcr.microsoft.com/devcontainers/cpp:ubuntu-24.04

# Avoid warnings by switching to noninteractive
ENV DEBIAN_FRONTEND=noninteractive

# Configure apt and install packages
RUN apt-get update \
    && apt-get -y install --no-install-recommends apt-utils dialog 2>&1 \
    # Install C++ tools
    && apt-get -y install \
        build-essential \
        cmake \
        ninja-build \
        gdb \
        pkg-config \
        pipx \
        # Install git, process tools, lsb-release (useful for CLI installs)
        git \
        openssh-client \
        less \
        iproute2 \
        procps \
        lsb-release \
        ca-certificates \
        # Install C++ compilers
        gcc \
        g++ \
        clang \
        # Install utilities
        curl \
        wget \
        unzip \
        tar \
        vim \
        nano \
        sudo \
    # Clean up
    && apt-get autoremove -y \
    && apt-get clean -y \
    && rm -rf /var/lib/apt/lists/*

# Switch back to dialog for any ad-hoc use of apt-get
ENV DEBIAN_FRONTEND=dialog

# Create a non-root user to use if preferred
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN groupadd --gid $USER_GID $USERNAME || true \
    && useradd -s /bin/bash --uid $USER_UID --gid $USER_GID -m $USERNAME || true \
    # Add sudo support for the non-root user
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

# Verify that cmake, ninja are installed correctly
RUN cmake --version && ninja --version

USER $USERNAME

RUN pipx ensurepath \
    && pipx install conan
