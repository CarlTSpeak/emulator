include_guard()
include(CheckCXXCompilerFlag)

##########################################
# System identification

set(OSX OFF)
set(LINUX OFF)
set(WIN OFF)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(LINUX ON)
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(OSX ON)
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(WIN ON)
endif()

##########################################

cmake_policy(SET CMP0069 NEW) 
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

##########################################

if(NOT CMAKE_SYSTEM_NAME MATCHES "Emscripten")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

##########################################

if(MOMO_BUILD_AS_LIBRARY)
    add_compile_definitions(MOMO_BUILD_AS_LIBRARY=1)
else()
    add_compile_definitions(MOMO_BUILD_AS_LIBRARY=0)
endif()

##########################################

if(MOMO_ENABLE_RUST_CODE)
  add_compile_definitions(MOMO_ENABLE_RUST_CODE=1)
else()
  add_compile_definitions(MOMO_ENABLE_RUST_CODE=0)
endif()

##########################################

if(UNIX)
  momo_add_c_and_cxx_compile_options(
    -fvisibility=hidden
    -ftrivial-auto-var-init=zero
  )
endif()

##########################################

if(LINUX)
  add_link_options(
    -Wl,--no-undefined
    -Wl,--gc-sections
    -Wl,-z,now
    -Wl,-z,noexecstack
    -static-libstdc++
  )

  momo_add_c_and_cxx_compile_options(
    -ffunction-sections
    -fdata-sections
    -fstack-protector-strong
    -fdiagnostics-color=always
  )

  add_compile_definitions(
    _REENTRANT
    _THREAD_SAFE
  )

  if(NOT MOMO_ENABLE_SANITIZER)
    add_compile_definitions(
      _FORTIFY_SOURCE=2
    )
  endif()

  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")
endif()

##########################################

if(CMAKE_SYSTEM_NAME MATCHES "Emscripten")
  momo_add_c_and_cxx_compile_options(
    -fexceptions
    -ftrivial-auto-var-init=zero
  )

  add_link_options(
    -fexceptions
    -sALLOW_MEMORY_GROWTH=1
    -sASSERTIONS
    -sWASM_BIGINT
    -sUSE_OFFSET_CONVERTER
    #-sEXCEPTION_CATCHING_ALLOWED=[..]
    -sEXIT_RUNTIME
    #-sASYNCIFY
  )

  if(MOMO_EMSCRIPTEN_SUPPORT_NODEJS)
    add_link_options(
      -lnodefs.js -sNODERAWFS=1
      -sENVIRONMENT=node
      -sMAXIMUM_MEMORY=4gb
      --pre-js ${CMAKE_CURRENT_LIST_DIR}/misc/node-pre-script.js
    )
  else() 
    add_link_options(
      -sENVIRONMENT=worker
    )
  endif()
endif()

##########################################

if(MSVC)
  string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  string(REPLACE "/EHs" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

  momo_add_c_and_cxx_compile_options(
    /sdl
    /GS
    /Gy
    /EHa
    #/guard:cf
  )

  momo_add_compile_options(CXX
    /Zc:__cplusplus
  )

  add_link_options(
    /INCREMENTAL:NO
  )

  momo_add_c_and_cxx_release_compile_options(
    /Ob2
  )

  add_compile_definitions(
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_WARNINGS
  )
endif()

##########################################

if(MOMO_ENABLE_AVX2 AND NOT (CMAKE_SYSTEM_NAME STREQUAL "Android"))
  set(CMAKE_REQUIRED_FLAGS -Werror)
  check_cxx_compiler_flag(-mavx2 COMPILER_SUPPORTS_MAVX2)
  set(CMAKE_REQUIRED_FLAGS "")

  check_cxx_compiler_flag(/arch:AVX2 COMPILER_SUPPORTS_ARCH_AVX2)

  if(COMPILER_SUPPORTS_MAVX2)
    momo_add_c_and_cxx_compile_options(-mavx2)
  endif()

  if (COMPILER_SUPPORTS_ARCH_AVX2)
    momo_add_c_and_cxx_compile_options(/arch:AVX2)
  endif()
endif()

##########################################

if(MOMO_ENABLE_SANITIZER)
  momo_add_c_and_cxx_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
endif()

##########################################
# Must be a dynamic runtime (/MD or /MDd) to enforce
# shared allocators between emulator and implementation

set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>DLL)

##########################################

if(MSVC)
  add_link_options(
    $<$<NOT:$<STREQUAL:${CMAKE_MSVC_RUNTIME_LIBRARY},MultiThreaded>>:/NODEFAULTLIB:libcmt.lib>
    $<$<NOT:$<STREQUAL:${CMAKE_MSVC_RUNTIME_LIBRARY},MultiThreadedDLL>>:/NODEFAULTLIB:msvcrt.lib>
    $<$<NOT:$<STREQUAL:${CMAKE_MSVC_RUNTIME_LIBRARY},MultiThreadedDebug>>:/NODEFAULTLIB:libcmtd.lib>
    $<$<NOT:$<STREQUAL:${CMAKE_MSVC_RUNTIME_LIBRARY},MultiThreadedDebugDLL>>:/NODEFAULTLIB:msvcrtd.lib>
  )
endif()

##########################################

set(OPT_DEBUG "-O0 -g")
set(OPT_RELEASE "-O3 -g")

if(MSVC)
  set(OPT_DEBUG "/Od /Ob0 /Zi")
  set(OPT_RELEASE "/O2 /Ob2 /Zi")

  add_link_options(/DEBUG)
endif()

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${OPT_DEBUG}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${OPT_DEBUG}")

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${OPT_RELEASE}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${OPT_RELEASE}")

##########################################

if(CMAKE_GENERATOR MATCHES "Visual Studio")
  momo_add_c_and_cxx_compile_options(/MP)
endif()
