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

# Set your build folder name
BUILD_FOLDER_NAME="scalehls-build"
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
    -S "${SCALEHLS_DIR}"  \
    -B .    \
    -DCMAKE_BUILD_TYPE=RELEASE \
    -DCMAKE_INSTALL_PREFIX="${INSTALLATION_FOLDER_DIR}"  \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DLLVM_USE_LINKER=lld \
    -DLLVM_PARALLEL_LINK_JOBS=2 \
    -DMLIR_DIR="${POLYGEIST_LLVM_BUILD_DIR}/lib/cmake/mlir" \
    -DLLVM_DIR="${POLYGEIST_LLVM_BUILD_DIR}/lib/cmake/llvm" \
    -DLLVM_EXTERNAL_LIT="${POLYGEIST_LLVM_BUILD_DIR}/bin/llvm-lit"


# cmake -G Ninja -S "${SCALEHLS_DIR}" B .

cmake --build . --target check-scalehls
ninja install
