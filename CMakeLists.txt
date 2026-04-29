cmake_minimum_required(VERSION 3.24)
project(CppTeamWorkSystemLab LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    set(DEFAULT_ZIG "E:/toolchains/zig-0.17.0-dev.135+9df02121d/zig.exe")
    set(DEFAULT_CARGO "E:/toolchains/cargo/bin/cargo.exe")
    set(DEFAULT_RUSTUP_HOME "E:/toolchains/rustup")
    set(DEFAULT_CARGO_HOME "E:/toolchains/cargo")
    set(DEFAULT_V "E:/v_go_ds50_benchmark/tools/v/v.exe")
else()
    set(DEFAULT_ZIG "")
    set(DEFAULT_CARGO "")
    set(DEFAULT_RUSTUP_HOME "")
    set(DEFAULT_CARGO_HOME "")
    set(DEFAULT_V "")
endif()

if(APPLE)
    set(MAC_TOOL_ROOT "$ENV{HOME}/.cppteamwork-toolchains")
    file(GLOB MAC_ZIG_DIRS LIST_DIRECTORIES true "${MAC_TOOL_ROOT}/zig-*")
    list(SORT MAC_ZIG_DIRS ORDER DESCENDING)
    foreach(candidate IN LISTS MAC_ZIG_DIRS)
        if(EXISTS "${candidate}/zig")
            set(DEFAULT_ZIG "${candidate}/zig")
            break()
        endif()
    endforeach()
    if(EXISTS "${MAC_TOOL_ROOT}/cargo/bin/cargo")
        set(DEFAULT_CARGO "${MAC_TOOL_ROOT}/cargo/bin/cargo")
        set(DEFAULT_RUSTUP_HOME "${MAC_TOOL_ROOT}/rustup")
        set(DEFAULT_CARGO_HOME "${MAC_TOOL_ROOT}/cargo")
    endif()
    if(EXISTS "${MAC_TOOL_ROOT}/v/v")
        set(DEFAULT_V "${MAC_TOOL_ROOT}/v/v")
    endif()
endif()

set(ZIG_EXECUTABLE "${DEFAULT_ZIG}" CACHE FILEPATH "Path to zig.exe")
set(CARGO_EXECUTABLE "${DEFAULT_CARGO}" CACHE FILEPATH "Path to cargo.exe")
set(RUSTUP_HOME_DIR "${DEFAULT_RUSTUP_HOME}" CACHE PATH "Rustup home used by the local toolchain")
set(CARGO_HOME_DIR "${DEFAULT_CARGO_HOME}" CACHE PATH "Cargo home used by the local toolchain")
set(V_EXECUTABLE "${DEFAULT_V}" CACHE FILEPATH "Path to v.exe")

if(NOT ZIG_EXECUTABLE)
    find_program(ZIG_EXECUTABLE NAMES zig)
endif()
if(NOT CARGO_EXECUTABLE)
    find_program(CARGO_EXECUTABLE NAMES cargo)
endif()
if(NOT V_EXECUTABLE)
    find_program(V_EXECUTABLE NAMES v HINTS "$ENV{HOME}/v" "/opt/homebrew/bin" "/usr/local/bin")
endif()

if(NOT ZIG_EXECUTABLE OR NOT EXISTS "${ZIG_EXECUTABLE}")
    message(FATAL_ERROR "Zig not found. Run scripts/bootstrap_toolchains.ps1 on Windows or scripts/bootstrap_toolchains.sh on macOS, or set -DZIG_EXECUTABLE=...")
endif()
if(NOT CARGO_EXECUTABLE OR NOT EXISTS "${CARGO_EXECUTABLE}")
    message(FATAL_ERROR "Cargo not found. Run scripts/bootstrap_toolchains.ps1 on Windows or scripts/bootstrap_toolchains.sh on macOS, or set -DCARGO_EXECUTABLE=...")
endif()
if(NOT V_EXECUTABLE OR NOT EXISTS "${V_EXECUTABLE}")
    message(FATAL_ERROR "V not found at ${V_EXECUTABLE}. Set -DV_EXECUTABLE=...")
endif()

set(CARGO_ENV)
if(RUSTUP_HOME_DIR)
    list(APPEND CARGO_ENV "RUSTUP_HOME=${RUSTUP_HOME_DIR}")
endif()
if(CARGO_HOME_DIR)
    list(APPEND CARGO_ENV "CARGO_HOME=${CARGO_HOME_DIR}")
endif()

add_library(expense_c STATIC src/c/expense_core.c)
target_include_directories(expense_c PUBLIC include)

set(RUST_LIB_DIR "${CMAKE_BINARY_DIR}/rust/release")
set(RUST_LIB "${RUST_LIB_DIR}/libexpense_store.a")
add_custom_command(
    OUTPUT "${RUST_LIB}"
    COMMAND "${CMAKE_COMMAND}" -E env
            ${CARGO_ENV}
            "${CARGO_EXECUTABLE}" build --release --manifest-path "${CMAKE_SOURCE_DIR}/src/rust/Cargo.toml" --target-dir "${CMAKE_BINARY_DIR}/rust"
    DEPENDS src/rust/Cargo.toml src/rust/src/lib.rs
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    VERBATIM
)
add_custom_target(expense_rust DEPENDS "${RUST_LIB}")

set(ZIG_LIB "${CMAKE_BINARY_DIR}/zig/libexpense_zig.a")
add_custom_command(
    OUTPUT "${ZIG_LIB}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/zig"
    COMMAND "${ZIG_EXECUTABLE}" build-lib -static -O ReleaseSafe
            "-femit-bin=${ZIG_LIB}"
            "${CMAKE_SOURCE_DIR}/src/zig/analytics.zig"
    DEPENDS src/zig/analytics.zig
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    VERBATIM
)
add_custom_target(expense_zig DEPENDS "${ZIG_LIB}")

set(V_C "${CMAKE_BINARY_DIR}/v/expense_v_text.c")
set(V_OBJ "${CMAKE_BINARY_DIR}/v/expense_v_text${CMAKE_C_OUTPUT_EXTENSION}")
add_custom_command(
    OUTPUT "${V_C}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/v"
    COMMAND "${V_EXECUTABLE}" -o "${V_C}" "${CMAKE_SOURCE_DIR}/src/v/text.v"
    DEPENDS src/v/text.v
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    VERBATIM
)
add_custom_command(
    OUTPUT "${V_OBJ}"
    COMMAND "${CMAKE_C_COMPILER}" -std=c99 -O2 -w
            -I "${CMAKE_SOURCE_DIR}/include" -c "${V_C}" -o "${V_OBJ}"
    DEPENDS "${V_C}"
    VERBATIM
)
add_custom_target(expense_v DEPENDS "${V_OBJ}")

add_executable(expense_app src/cpp/main.cpp "${V_OBJ}")
add_dependencies(expense_app expense_rust expense_zig expense_v)
target_include_directories(expense_app PRIVATE include)
target_link_libraries(expense_app PRIVATE expense_c "${ZIG_LIB}" "${RUST_LIB}")

if(MINGW)
    target_link_options(expense_app PRIVATE -static-libgcc -static-libstdc++)
endif()

if(WIN32)
    target_link_libraries(expense_app PRIVATE ws2_32 bcrypt userenv advapi32 ntdll)
elseif(APPLE)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    find_library(SECURITY_FRAMEWORK Security)
    find_library(SYSTEMCONFIGURATION_FRAMEWORK SystemConfiguration)
    target_link_libraries(expense_app PRIVATE
        ${COREFOUNDATION_FRAMEWORK}
        ${SECURITY_FRAMEWORK}
        ${SYSTEMCONFIGURATION_FRAMEWORK}
        iconv
        resolv
    )
else()
    find_package(Threads REQUIRED)
    target_link_libraries(expense_app PRIVATE Threads::Threads dl m)
endif()
