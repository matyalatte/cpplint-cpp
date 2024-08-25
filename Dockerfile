# Building workflow for musl build.
#
# 1. Use this docker file to build the executable.
#    docker build -t cpplint ./
#
# 2. Run the built image.
#    docker run --name cpplint cpplint
#
# 3. Use "docker cp" to get the built executable.
#    docker cp cpplint:/cpplint-cpp/dist ./
#
# Notes:
#   -You can use buildx for cross compiling
#    sudo apt install -y qemu-user-static binfmt-support
#    docker buildx build --platform linux/arm64 -t cpplint_arm ./

# Base image
FROM alpine:3.18.8

# Install packages
RUN apk update && \
    apk add --no-cache \
        alpine-sdk linux-headers bash \
        py3-pip python3

# Install meson
RUN pip3 install meson==1.3.1 ninja==1.11.1 build

# Clone the repo
COPY . /cpplint-cpp

# Build
WORKDIR /cpplint-cpp
RUN meson setup build --native-file=presets/release.ini && \
    meson compile -C build && \
    meson test -C build && \
    strip --strip-all ./build/cpplint-cpp

# Make wheel package
WORKDIR /cpplint-cpp
RUN mkdir dist && \
    cp ./build/cpplint-cpp dist && \
    cp ./build/version.h dist && \
    python3 -m build
