cmake_minimum_required(VERSION 3.21)
project(CrystalDiskInfoLinux VERSION 9.9.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Upstream CrystalDiskInfo sources (unzipped CrystalDiskInfo*Src.zip)
set(CDI_UPSTREAM "${CMAKE_CURRENT_SOURCE_DIR}/../upstream" CACHE PATH "Upstream CrystalDiskInfo source directory")
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# --- core: upstream AtaSmart logic + Linux back end ------------------------
set(CORE_GEN "${CMAKE_CURRENT_BINARY_DIR}/AtaSmartCore.cpp")
add_custom_command(
    OUTPUT "${CORE_GEN}"
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/extract_core.py" "${CDI_UPSTREAM}/AtaSmart.cpp" "${CORE_GEN}"
    DEPENDS "${CDI_UPSTREAM}/AtaSmart.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/tools/extract_core.py"
    COMMENT "Extracting portable logic from upstream AtaSmart.cpp")

add_library(rawio STATIC core/RawIo.cpp)
target_include_directories(rawio PUBLIC core)

add_library(cdicore STATIC
    "${CORE_GEN}"
    core/AtaSmartLinux.cpp
    "${CDI_UPSTREAM}/NVMeInterpreter.cpp")
target_include_directories(cdicore PUBLIC shim core "${CDI_UPSTREAM}")
# _M_ARM drops the AMD RAIDXpert2 (Windows DLL) paths upstream already guards.
target_compile_definitions(cdicore PRIVATE _M_ARM PUBLIC UNICODE _UNICODE)
target_compile_options(cdicore PRIVATE -fpermissive -w)
target_link_libraries(cdicore PUBLIC rawio)

# --- privileged helper -----------------------------------------------------
add_executable(cdi-probe probe/main.cpp)
target_link_libraries(cdi-probe PRIVATE rawio)

option(CDI_BUILD_GUI "Build the Qt GUI" ON)
if(CDI_BUILD_GUI)
    add_subdirectory(app)
endif()

install(TARGETS cdi-probe RUNTIME DESTINATION libexec/crystaldiskinfo)

install(FILES packaging/crystaldiskinfo.desktop DESTINATION share/applications)
install(FILES packaging/org.crystalmark.crystaldiskinfo.policy DESTINATION share/polkit-1/actions)
