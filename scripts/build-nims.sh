#!/bin/bash

# Create absolute paths, using the trunk/scripts directory
# as the base. This will put build products alongside the
# source checkout directories.

function abspath {
    if [[ -d "$1" ]]
    then
        pushd "$1" >/dev/null
        pwd
        popd >/dev/null
    elif [[ -e $1 ]]
    then
        pushd "$(dirname "$1")" >/dev/null
        echo "$(pwd)/$(basename "$1")"
        popd >/dev/null
    else
        echo "$1" does not exist! >&2
        return 127
    fi
}

# this will be an absolute path, so now we can run the
# build script from any directory
SCRIPTS_DIR=$(abspath $(dirname $0))
BUILD_DIR=$SCRIPTS_DIR/../build
SOURCE_DIR=$SCRIPTS_DIR/../nims-source
YAML_SRC=$SCRIPTS_DIR/../vendorsrc/yaml-cpp-release-0.5.2

# TODO:  need to check version of yaml lib
if ! [ -f "/usr/local/lib/libyaml-cpp.a" ]; then
    echo "*** building libyaml-cpp.a ***"

    if ! [ -d "$YAML_SRC/build" ]; then
        echo "*** configuring yaml-cpp ***"
        cd "$YAML_SRC"
        mkdir build
        cd build
        cmake ../ && make
        echo "*** You now need to run 'sudo make install' in $(pwd) ***"
        exit 0
    fi
fi

pushd .
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# alternate location for boost
if [ -d /usr/local/boost ]; then
    export CMAKE_LIBRARY_PATH="/usr/local/boost/lib:$CMAKE_LIBRARY_PATH"
    export CMAKE_INCLUDE_PATH="/usr/local/boost/include:$CMAKE_INCLUDE_PATH"
fi

# boost and cmake is FUBAR on RHEL6
# passing -DBoost_NO_BOOST_CMAKE=ON works around this
# TODO:  Make the build type a command line arg, so can make a Release build
cmake $SOURCE_DIR -DBoost_NO_BOOST_CMAKE=ON -DCMAKE_BUILD_TYPE=Debug && make
popd
