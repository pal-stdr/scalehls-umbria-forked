# The absolute path to the directory of this script.
BUILD_SCRIPT_ROOT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Project root dir (i.e. polsca/)
SCALEHLS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Go to the llvm directory and carry out installation.
# You need all the clang bin and lib in env
POLYGEIST_LLVM_BUILD_DIR="${SCALEHLS_DIR}/llvm-16-src-build-for-scalehls"
POLYGEIST_LLVM_BUILD_LIB_DIR="${POLYGEIST_LLVM_BUILD_DIR}/lib"
POLYGEIST_LLVM_BUILD_BIN_DIR="${POLYGEIST_LLVM_BUILD_DIR}/bin"

export LD_LIBRARY_PATH="${POLYGEIST_LLVM_BUILD_LIB_DIR}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PATH="${POLYGEIST_LLVM_BUILD_BIN_DIR}${PATH:+:${PATH}}"

echo $PATH


POLYGEIST_DIR="${SCALEHLS_DIR}/polygeist"


# Set your build folder name
BUILD_FOLDER_NAME="polygeist-build-for-scalehls"
INSTALLATION_FOLDER_NAME="${BUILD_FOLDER_NAME}-installation"

# Create the build folders in $SCALEHLS_DIR
BUILD_FOLDER_DIR="${SCALEHLS_DIR}/${BUILD_FOLDER_NAME}"
INSTALLATION_FOLDER_DIR="${SCALEHLS_DIR}/${INSTALLATION_FOLDER_NAME}"


echo $POLYGEIST_LLVM_DIR
echo $BUILD_FOLDER_DIR


rm -Rf "${BUILD_FOLDER_DIR}" "${INSTALLATION_FOLDER_DIR}"
mkdir -p "${BUILD_FOLDER_DIR}" "${INSTALLATION_FOLDER_DIR}"
cd "${BUILD_FOLDER_DIR}/"



cmake   \
    -G Ninja    \
    -S "${POLYGEIST_DIR}"  \
    -B .    \
    -DCMAKE_BUILD_TYPE=DEBUG \
    -DCMAKE_INSTALL_PREFIX="${INSTALLATION_FOLDER_DIR}"  \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DLLVM_USE_LINKER=lld \
    -DMLIR_DIR="${POLYGEIST_LLVM_BUILD_DIR}/lib/cmake/mlir" \
    -DCLANG_DIR="${POLYGEIST_LLVM_BUILD_DIR}/lib/cmake/clang" \
    -DLLVM_ENABLE_ASSERTIONS=ON

cmake --build .
cmake --build . --target check-mlir-clang
ninja install
