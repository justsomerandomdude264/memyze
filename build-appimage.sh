#!/bin/bash
set -e

# Configuration
APP_NAME="memyze"
VERSION="0.1"
BUILD_DIR="build-appimage"
APPDIR="${BUILD_DIR}/AppDir"

# Clean previous build
rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR}

# Build the application
cmake -B ${BUILD_DIR} -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr
cmake --build ${BUILD_DIR}

# Install to AppDir
DESTDIR=${APPDIR} cmake --install ${BUILD_DIR}

# Create AppDir structure
mkdir -p ${APPDIR}/usr/share/applications
mkdir -p ${APPDIR}/usr/share/icons/hicolor/256x256/apps

# Copy desktop file
cp packaging/memyze.desktop ${APPDIR}/usr/share/applications/

# Copy icon
cp assets/logo.png ${APPDIR}/usr/share/icons/hicolor/256x256/apps/memyze.png

# Create symlinks 
cd ${APPDIR}
ln -sf usr/share/applications/memyze.desktop memyze.desktop
ln -sf usr/share/icons/hicolor/256x256/apps/memyze.png memyze.png
ln -sf usr/share/icons/hicolor/256x256/apps/memyze.png .DirIcon
ln -sf usr/bin/memyze AppRun
cd -

# Download linuxdeploy and Qt plugin
wget -N https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
wget -N https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x linuxdeploy*.AppImage

# Build AppImage
export QMAKE=$(which qmake6)
./linuxdeploy-x86_64.AppImage \
    --appdir=${APPDIR} \
    --desktop-file=${APPDIR}/usr/share/applications/memyze.desktop \
    --icon-file=${APPDIR}/usr/share/icons/hicolor/256x256/apps/memyze.png \
    --plugin qt \
    --output appimage

# The AppImage will be created as Memyze-0.1-x86_64.AppImage
echo "AppImage created successfully!"
