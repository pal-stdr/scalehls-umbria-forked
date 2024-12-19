
# Please use -DLLVM_PARALLEL_LINK_JOBS=2 so that your build doesn't crash because of the RAM overflow
# https://reviews.llvm.org/D72402
# With ld as a linker (version: 2.33.1) ==== (Doesn't work)
# -DLLVM_PARALLEL_LINK_JOBS=any => out of memory
# -DLLVM_PARALLEL_LINK_JOBS=5 => no more than 30 GB memory
# -DLLVM_PARALLEL_LINK_JOBS=2 => no more than 14 GB memory
# -DLLVM_PARALLEL_LINK_JOBS=1 => no more than 10 GB memory

# With lld as a linker (version: 2.33.1) => -DLLVM_USE_LINKER=lld
# ====

# -DLLVM_PARALLEL_LINK_JOBS=any => no more then 9 GB memory (Doesn't work)
# -DLLVM_PARALLEL_LINK_JOBS=2 => no more than 6 GB memory

# The current defaults for LLVM_PARALLEL_LINK_JOBS is empty, meaning any number (only limited by ninja/make parallel option).

# The LLVM_PARALLEL_LINK_JOBS=2 is a better default option, if the linker is not lld.



# The absolute path to the directory of this script.
BUILD_SCRIPT_ROOT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Project root dir (i.e. polsca/)
SCALEHLS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Go to the llvm directory and carry out installation.
POLYGEIST_LLVM_DIR="${SCALEHLS_DIR}/polygeist/llvm-project"


# Set your build folder name
BUILD_FOLDER_NAME="llvm-16-src-build-for-scalehls"
INSTALLATION_FOLDER_NAME="${BUILD_FOLDER_NAME}-installation"

# Create the build folders in $SCALEHLS_DIR
BUILD_FOLDER_DIR="${SCALEHLS_DIR}/${BUILD_FOLDER_NAME}"
INSTALLATION_FOLDER_DIR="${SCALEHLS_DIR}/${INSTALLATION_FOLDER_NAME}"


echo $POLYGEIST_LLVM_DIR
echo $BUILD_FOLDER_DIR


# Mandatory for building llvm with clang.
# You need all the clang bin and lib in env
LLVM_PREBUILT_DIR="${MY_EXTERNAL_SDD_WORK_DIR}/compiler-projects/llvm-16-src-build/build"
LLVM_PREBUILT_LIB_DIR="${LLVM_PREBUILT_DIR}/lib"
LLVM_PREBUILT_BIN_DIR="${LLVM_PREBUILT_DIR}/bin"

export LD_LIBRARY_PATH="${LLVM_PREBUILT_LIB_DIR}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PATH="${LLVM_PREBUILT_BIN_DIR}${PATH:+:${PATH}}"


rm -Rf "${BUILD_FOLDER_DIR}" "${INSTALLATION_FOLDER_DIR}"
mkdir -p "${BUILD_FOLDER_DIR}" "${INSTALLATION_FOLDER_DIR}"
cd "${BUILD_FOLDER_DIR}"/




# Works
# cmake   \
#     -G Ninja    \
#     -S "${POLYGEIST_LLVM_DIR}/llvm"  \
#     -B .    \
#     -DCMAKE_BUILD_TYPE=DEBUG      \
#     -DCMAKE_INSTALL_PREFIX="${INSTALLATION_FOLDER_DIR}"  \
#     -DLLVM_ENABLE_PROJECTS="mlir;clang;lld" \
#     -DLLVM_TARGETS_TO_BUILD="host" \
#     -DLLVM_ENABLE_ASSERTIONS=ON \
#     -DMLIR_ENABLE_BINDINGS_PYTHON="${PYBIND:=OFF}" \
#     -DSCALEHLS_ENABLE_BINDINGS_PYTHON="${PYBIND:=OFF}" \
#     -DLLVM_PARALLEL_LINK_JOBS=2 \
#     -DCMAKE_C_COMPILER=clang \
#     -DCMAKE_CXX_COMPILER=clang++    \
#     -DLLVM_USE_LINKER=lld




cmake   \
    -G Ninja    \
    -S "${POLYGEIST_LLVM_DIR}/llvm"  \
    -B .    \
    -DCMAKE_BUILD_TYPE=DEBUG      \
    -DCMAKE_INSTALL_PREFIX="${INSTALLATION_FOLDER_DIR}"  \
    -DLLVM_ENABLE_PROJECTS="mlir;clang;lld" \
    -DLLVM_TARGETS_TO_BUILD="host" \
    -DLLVM_ENABLE_ASSERTIONS=ON \
    -DMLIR_ENABLE_BINDINGS_PYTHON="${PYBIND:=OFF}" \
    -DSCALEHLS_ENABLE_BINDINGS_PYTHON="${PYBIND:=OFF}" \
    -DLLVM_PARALLEL_LINK_JOBS=2 \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++




# Run build
cmake --build .
ninja install
