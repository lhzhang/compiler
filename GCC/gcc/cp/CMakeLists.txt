# Restore the real executable output path.  We want to put our executable
# there.
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${GCC_EXECUTABLE_OUTPUT_PATH})

INCLUDE_DIRECTORIES(BEFORE ${CMAKE_CURRENT_SOURCE_DIR})

#-----------------------------------------------------------------------------
# C++ Compiler Executable

IF(CMAKE_SYSTEM MATCHES "HP-UX")
  IF(CMAKE_C_COMPILER MATCHES "(^cc$|/cc$)")
    SET_SOURCE_FILES_PROPERTIES(../c-opts.c PROPERTIES COMPILE_FLAGS "-Wp,-H,20000")
  ENDIF(CMAKE_C_COMPILER MATCHES "(^cc$|/cc$)")
ENDIF(CMAKE_SYSTEM MATCHES "HP-UX")

#-----------------------------------------------------------------------------
# Special flags for some source files.

IF(CYGWIN)
  SET_SOURCE_FILES_PROPERTIES(../cppdefault.c PROPERTIES
    COMPILE_FLAGS "-DTOOL_INCLUDE_DIR=/usr/include")
ENDIF(CYGWIN)

INCLUDE_DIRECTORIES(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
CONFIGURE_FILE(${GCC_SOURCE_DIR}/gcc/path64_prefix.h.in
               ${GCC_BINARY_DIR}/gcc/cp/path64_prefix.h @ONLY IMMEDIATE)

PATH64_SET_MULTIARCH_SOURCES(cc1plus42_sources COMMON
     ${cxx_target_srcs}         ../attribs.c        ../c-common.c    ../c-cppbuiltin.c
            ../c-dump.c        ../c-format.c      ../c-gimplify.c       ../c-incpath.c
             ../c-lex.c           ../c-omp.c          ../c-opts.c           ../c-pch.c
        ../c-ppoutput.c        ../c-pragma.c  ../c-pretty-print.c     ../c-semantics.c
        ../cppdefault.c  ../dummy-checksum.c            ../main.c          ../prefix.c
         ../stub-objc.c     ../tree-inline.c    ../tree-mudflap.c               call.c
                class.c        cp-gimplify.c            cp-lang.c    cp-objcp-common.c
                  cvt.c   cxx-pretty-print.c               decl.c              decl2.c
                 dump.c              error.c             except.c               expr.c
               friend.c               init.c                lex.c             mangle.c
               method.c        name-lookup.c           optimize.c             parser.c
                   pt.c              ptree.c               repo.c               rtti.c
               search.c          semantics.c               tree.c             typeck.c
              typeck2.c


   # source file count: 53
  )


PATH64_ADD_MULTIARCH_EXECUTABLE(cc1plus42 cc1plus42_sources)
PATH64_MULTIARCH_LINK_MULTIARCH_LIBRARIES(cc1plus42 backend)
PATH64_MULTIARCH_LINK_LIBRARIES(cc1plus42 cpp decnumber iberty )
path64_multiarch_link_multiarch_libraries( cc1plus42 spin)
path64_install_multiarch(cc1plus42)
path64_add_dependencies_from_multiarch( compiler-stage cc1plus42 )

IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    PATH64_MULTIARCH_LINK_LIBRARIES(cc1plus42 winlib)
ENDIF()


# Use the "-lx" option to disable map file generation on Borland.
# This avoids the "Fatal: Access violation.  Link terminated." error.
IF(BORLAND)
  PATH64_SET_MULTIARCH_PROPERTY(cc1plus42 LINK_FLAGS "-lx")
ENDIF(BORLAND)

