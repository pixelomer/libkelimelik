#!/bin/bash

examples=(account-info proxy tests)

if [ -z "${PWD}" ]; then
  echo "\$PWD appears to be empty/unset. This should never happen."
  exit 1;
fi

PROJECT_ROOT="${PWD}"

clean_dir() {
  echo "Cleaning up..."
  pushd "${PROJECT_ROOT}/out" > /dev/null
    rm -f ${examples[@]} *.o libkelimelik.a src headers
  popd > /dev/null
}

clean_dir
trap 'a=$?; echo "[ERROR] One of the build steps failed (make.sh:${LINENO})"; clean_dir; exit ${a};' ERR

# Create output directory and switch to it
mkdir -p "${PROJECT_ROOT}/out"
cd "${PROJECT_ROOT}/out"

# Create temporary symlinks
ln -s ../src src
ln -s ../headers headers

# Build libkelimelik
echo "Building libkelimelik..."
clang -Iheaders -c src/*.c

# Remove the symlinks
rm -f src headers

# Create libkelimelik.a with object files
echo "Creating libkelimelik archive..."
ar -r libkelimelik.a *.o 2>/dev/null >&2

# Delete object files
rm -f *.o

# Switch back to project root
cd "${PROJECT_ROOT}"

# Build examples
for example in "${examples[@]}"; do
  echo "Building ${example}..."
  clang -Wall -O2 -Iheaders examples/"${example}"/*.c "${PROJECT_ROOT}/out/libkelimelik.a" -o "${PROJECT_ROOT}/out/${example}"
done