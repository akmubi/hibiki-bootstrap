cmake_minimum_required(VERSION 3.20)

project(hibiki_bootstrap LANGUAGES C)

if (MSVC)
  # Workaround for CMake+MSVC bug:
  # MASM does NOT support *DLL runtime values, so give it /MT[/MTd].
  set(CMAKE_ASM_MASM_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  enable_language(ASM_MASM)
else()
  # MinGW / cross build uses generic ASM
  enable_language(ASM)
endif()

add_executable(gen_proxy
  dll_proxy_gen.c
)

target_link_libraries(gen_proxy PRIVATE dbghelp)

set(GEN_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN_OUT_DIR})

set(TARGET_DLL_NAME XAPOFX1_5)

if (WIN32 AND NOT CMAKE_CROSSCOMPILING)
  # Native build:
  # get the real DLL from the host
  if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(REAL_DLL "$ENV{SystemRoot}/System32/${TARGET_DLL_NAME}.dll")
  else()
    set(REAL_DLL "$ENV{SystemRoot}/SysWOW64/${TARGET_DLL_NAME}.dll")
  endif()
else()
  # Cross-build:
  # get the real DLL under the source tree
  set(REAL_DLL "${CMAKE_SOURCE_DIR}/external/${TARGET_DLL_NAME}.dll")
endif()

set(GEN_DEF  ${GEN_OUT_DIR}/${TARGET_DLL_NAME}.def)
set(GEN_MASM ${GEN_OUT_DIR}/${TARGET_DLL_NAME}.asm)
set(GEN_GAS  ${GEN_OUT_DIR}/${TARGET_DLL_NAME}_gas.S)
set(GEN_C    ${GEN_OUT_DIR}/dllmain.c)

add_custom_command(
  OUTPUT ${GEN_DEF} ${GEN_MASM} ${GEN_GAS} ${GEN_C}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${GEN_OUT_DIR}
  COMMAND gen_proxy "${REAL_DLL}" "${GEN_OUT_DIR}" "hibiki_bootstrap.dll"
  DEPENDS gen_proxy
  VERBATIM
)

set_source_files_properties(
  ${GEN_C} ${GEN_MASM} ${GEN_GAS} ${GEN_DEF}
  PROPERTIES GENERATED TRUE
)

if (MSVC)
  add_library(${TARGET_DLL_NAME} SHARED
    ${GEN_C}
    ${GEN_MASM}
  )

  target_link_options(${TARGET_DLL_NAME} PRIVATE "/DEF:${GEN_DEF}" /NOIMPLIB)
  target_compile_definitions(${TARGET_DLL_NAME} PRIVATE WIN32_LEAN_AND_MEAN)
  target_link_libraries(${TARGET_DLL_NAME} PRIVATE Shlwapi)
else()
  add_library(${TARGET_DLL_NAME} SHARED
    ${GEN_C}
    ${GEN_DEF}
    ${GEN_GAS}
  )

  target_link_options(${TARGET_DLL_NAME} PRIVATE
    "-Wl,--enable-stdcall-fixup"
  )
  target_compile_definitions(${TARGET_DLL_NAME} PRIVATE WIN32_LEAN_AND_MEAN)
  target_link_libraries(${TARGET_DLL_NAME} PRIVATE shlwapi)

  set_target_properties(${TARGET_DLL_NAME} PROPERTIES
    PREFIX ""
    OUTPUT_NAME ${TARGET_DLL_NAME}
  )
endif()

add_library(hibiki_bootstrap SHARED
  dllmain.c
)

if (MSVC)
  foreach(flag_var
          CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
          CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS_MINSIZEREL
          CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
          CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_MINSIZEREL)
    if (DEFINED ${flag_var})
      string(REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
    endif()
  endforeach()

  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

  target_compile_options(hibiki_bootstrap PRIVATE /MT)

  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()
  set_target_properties(hibiki_bootstrap PROPERTIES
    PREFIX "" # drop "lib"
    OUTPUT_NAME hibiki_bootstrap
  )
endif()
