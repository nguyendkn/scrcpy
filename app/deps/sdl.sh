#!/usr/bin/env bash
set -ex
DEPS_DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DEPS_DIR"
. common
process_args "$@"

VERSION=3.2.18
FILENAME=SDL-$VERSION.tar.gz
PROJECT_DIR=SDL-release-$VERSION
SHA256SUM=51539fa13e546bc50c632beed3f34257de2baa38a4c642048de56377903b4265

cd "$SOURCES_DIR"

if [[ -d "$PROJECT_DIR" ]]
then
    echo "$PWD/$PROJECT_DIR" found
else
    get_file "https://github.com/libsdl-org/SDL/archive/refs/tags/release-$VERSION.tar.gz" "$FILENAME" "$SHA256SUM"
    tar xf "$FILENAME"  # First level directory is "$PROJECT_DIR"
fi

mkdir -p "$BUILD_DIR/$PROJECT_DIR"
cd "$BUILD_DIR/$PROJECT_DIR"

export CFLAGS='-O2'
export CXXFLAGS="$CFLAGS"

if [[ -d "$DIRNAME" ]]
then
    echo "'$PWD/$DIRNAME' already exists, not reconfigured"
    cd "$DIRNAME"
else
    mkdir "$DIRNAME"
    cd "$DIRNAME"

    conf=(
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR/$DIRNAME"
    )

    if [[ "$HOST" == linux ]]
    then
        conf+=(
            -DSDL_WAYLAND=ON
            -DSDL_X11=ON
        )
    fi

    if [[ "$LINK_TYPE" == static ]]
    then
        conf+=(
            -DBUILD_SHARED_LIBS=OFF
        )
    else
        conf+=(
            -DBUILD_SHARED_LIBS=ON
        )
    fi

    if [[ "$BUILD_TYPE" == cross ]]
    then
        if [[ "$HOST" = win32 ]]
        then
            TOOLCHAIN_FILENAME="cmake-toolchain-mingw64-i686.cmake"
        elif [[ "$HOST" = win64 ]]
        then
            TOOLCHAIN_FILENAME="cmake-toolchain-mingw64-x86_64.cmake"
        else
            echo "Unsupported cross-build to host: $HOST" >&2
            exit 1
        fi

        conf+=(
            -DCMAKE_TOOLCHAIN_FILE="$SOURCES_DIR/$PROJECT_DIR/build-scripts/$TOOLCHAIN_FILENAME"
        )
    fi

    cmake "$SOURCES_DIR/$PROJECT_DIR" "${conf[@]}"
fi

cmake --build .
cmake --install .
