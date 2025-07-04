# libime.a
add_library(libime STATIC
    cand_info.cpp
    comp_str.cpp
    config.cpp
    convert.cpp
    debug.cpp
    imm.cpp
    immsec.cpp
    input.cpp
    keychar.cpp
    main.cpp
    postal.cpp
    process.cpp
    regword.cpp
    ui.cpp
    uicand.cpp
    uicomp.cpp
    uiguide.cpp
    uistate.cpp)
set_target_properties(libime PROPERTIES PREFIX "")

# mzimeja.ime
add_library(ime SHARED mzimeja.def mzimeja_res.rc)
target_link_libraries(ime libime kernel32 user32 gdi32 advapi32 comctl32 imm32 shlwapi)
target_compile_options(ime PRIVATE -DIME_DLL=1)
set_target_properties(ime PROPERTIES PREFIX "")
set_target_properties(ime PROPERTIES SUFFIX "")
set_target_properties(ime PROPERTIES OUTPUT_NAME "mzimeja.ime")

# imetests.exe
add_executable(imetests tests.cpp mzimeja_res.rc)
target_link_libraries(imetests libime kernel32 user32 gdi32 advapi32 comctl32 imm32 shlwapi)

# do statically link
set_target_properties(ime PROPERTIES LINK_DEPENDS_NO_SHARED 1)
set_target_properties(ime PROPERTIES LINK_SEARCH_START_STATIC 1)
set_target_properties(ime PROPERTIES LINK_SEARCH_END_STATIC 1)
