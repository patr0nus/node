cmake_minimum_required(VERSION 3.13)

project(node)


add_library(node STATIC ${CMAKE_CURRENT_LIST_DIR}/dummy.c)

target_include_directories(node INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include/node)

if(WIN32)

  file(GLOB LIBNODE_ARCHIVES ${CMAKE_CURRENT_LIST_DIR}/static_libs/*.lib)
  file(GLOB LIBNODE_OBJS ${CMAKE_CURRENT_LIST_DIR}/static_libs/*.obj)
  target_link_libraries(node INTERFACE ${LIBNODE_ARCHIVES} ${LIBNODE_OBJS} ws2_32 dbghelp crypt32 winmm iphlpapi psapi userenv)

  if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  # Nodejs is compiled with /SAFESEH:NO with when using MSVC targeting 32bit: https://github.com/nodejs/node/commit/2f477bd34de128683c8d183e759b8f098205b1c9
    target_link_options(node INTERFACE "-SAFESEH:NO")
  endif()

elseif(UNIX)

  file(GLOB LIBNODE_ARCHIVES ${CMAKE_CURRENT_LIST_DIR}/static_libs/*.[a,o])

  if(APPLE)
    target_link_libraries(node INTERFACE ${LIBNODE_ARCHIVES})

    # Make the generated xcode project standalone (no need to link to nodejs' archives explicitly)
    list(JOIN LIBNODE_ARCHIVES " " LIBNODE_PRELINK_FLAGS)
    set_target_properties(node PROPERTIES
      XCODE_ATTRIBUTE_OTHER_LIBTOOLFLAGS ${LIBNODE_PRELINK_FLAGS}
    )

  else() # Linux

    target_link_libraries(node INTERFACE "-Wl,--start-group" ${LIBNODE_ARCHIVES} "-Wl,--end-group" "-pthread" "-ldl" "-lrt" "-lm" "-rdynamic")

  endif()
endif()
