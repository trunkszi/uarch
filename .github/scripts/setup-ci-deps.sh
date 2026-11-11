#!/usr/bin/env bash
set -euo pipefail

uv tool install 'cmake>=4.0,<5'
export PATH="$HOME/.local/bin:$PATH"
if [[ -n "${GITHUB_PATH:-}" ]]; then
    printf '%s\n' "$HOME/.local/bin" >> "$GITHUB_PATH"
fi
mkdir -p "$HOME/.cache/ccache" "$HOME/.cache/vcpkg/archives"
vcpkg_root="$HOME/.vcpkg"
if [[ ! -d "$vcpkg_root" ]]; then
    git clone --filter=blob:none https://github.com/microsoft/vcpkg.git "$vcpkg_root"
    git -C "$vcpkg_root" checkout --detach 2273a28f34ce5aac8be50b3b6b44da7fc1722e06
fi
if [[ ! -x "$vcpkg_root/vcpkg" ]]; then
    "$vcpkg_root/bootstrap-vcpkg.sh" -disableMetrics
fi
port="$HOME/.local/vcpkg/clapp"
if [[ ! -d "$port" ]]; then
    mkdir -p "$port"
    cat > "$port/vcpkg.json" <<'JSON'
{
  "name": "clapp",
  "version": "0.1.0",
  "description": "Header-only C++26 command-line parser",
  "license": "MIT",
  "supports": "linux",
  "dependencies": [
    { "name": "vcpkg-cmake", "host": true },
    { "name": "vcpkg-cmake-config", "host": true }
  ]
}
JSON
    cat > "$port/portfile.cmake" <<'CMAKE'
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
set(VCPKG_BUILD_TYPE release)
vcpkg_from_git(
  OUT_SOURCE_PATH SOURCE_PATH
  URL https://github.com/trunkszi/clapp.git
  REF f46c898c8dc997748361acb8519515989583b97c
  FETCH_REF main
  HEAD_REF main)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}" OPTIONS
  -DCMAKE_INSTALL_LIBDIR=lib
  -DCLAPP_BUILD_TESTS=OFF -DCLAPP_BUILD_EXAMPLES=OFF
  -DCLAPP_BUILD_BENCHES=OFF -DCLAPP_BUILD_INSTALL_TEST=OFF
  -DCLAPP_WERROR=OFF -DCLAPP_USE_CATCH2=OFF)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME clapp CONFIG_PATH lib/cmake/clapp)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug")
set(VCPKG_POLICY_EMPTY_PACKAGE enabled)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
CMAKE
fi
cmake --version
"$vcpkg_root/vcpkg" version
