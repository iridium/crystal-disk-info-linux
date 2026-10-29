# Build environment for the Linux port of CrystalDiskInfo.
# Arch base so the produced binary links against the same Qt the host runs.
# python-pefile extracts the voice clips, python-pillow converts the app icon.
FROM docker.io/library/archlinux:latest

RUN pacman -Syu --noconfirm --needed \
        base-devel cmake ninja python python-pefile python-pillow \
        qt6-base qt6-multimedia unzip \
    && rm -rf /var/cache/pacman/pkg/*

WORKDIR /work
