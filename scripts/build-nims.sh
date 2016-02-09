#!/bin/bash

# Create absolute paths, using the trunk/scripts directory
# as the base. This will put build products alongside the
# source checkout directories.
BUILD_DIR=$(pwd)/../build
SOURCE_DIR=$(pwd)/../nims-source
YAML_SRC=$(pwd)/../vendorsrc/yaml-cpp-release-0.5.2

# TODO:  need to check version of yaml lib
if ! [ -f "/usr/local/lib/libyaml-cpp.a" ]; then
    echo "*** building libyaml-cpp.a ***"
    if ! [ -d "$YAML_SRC" ]; then
        echo "*** checking out and configuring yaml ***"
        pushd .
        cd ..
        svn co https://subversion.pnnl.gov/svn/NIMS/vendorsrc/yaml-cpp-release-0.5.2
        popd
    fi

    if ! [ -d "$YAML_SRC/build" ]; then
        echo "*** configuring yaml-cpp ***"
        pushd .
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
