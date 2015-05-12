BUILD_DIR=build-fish

echo "*** needs to be fixed for subdirectory ***" >&2
exit 1

pushd .
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
export CMAKE_LIBRARY_PATH="/usr/local/boost/lib:$CMAKE_LIBRARY_PATH"
export CMAKE_INCLUDE_PATH="/usr/local/boost/include:$CMAKE_INCLUDE_PATH"
cmake ../FishTracker && make
popd
