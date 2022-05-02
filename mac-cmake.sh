eval "$(brew shellenv)"
cmake . -DLibArchive_LIBRARY=$HOMEBREW_PREFIX/opt/libarchive/lib/libarchive.dylib -DLibArchive_INCLUDE_DIR=$HOMEBREW_PREFIX/opt/libarchive/include -DCMAKE_BUILD_TYPE=Release