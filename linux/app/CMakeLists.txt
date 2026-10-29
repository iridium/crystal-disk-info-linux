find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)
set(CMAKE_AUTOMOC ON)

add_executable(crystaldiskinfo
    main.cpp
    MainWindow.cpp MainWindow.h
    DiskData.cpp DiskData.h
    Lang.cpp Lang.h
    Theme.cpp Theme.h)
target_link_libraries(crystaldiskinfo PRIVATE cdicore Qt6::Widgets Qt6::Multimedia)
# DiskData.cpp includes the Windows shim; keep its warnings quiet like the core.
set_source_files_properties(DiskData.cpp PROPERTIES COMPILE_OPTIONS "-fpermissive;-w")

install(TARGETS crystaldiskinfo RUNTIME DESTINATION bin)
