PROJECT(PATHSCALE)

SET(CMAKE_C_FLAGS "-D_LANGUAGE_C ${CMAKE_C_FLAGS}")
SET(CMAKE_CXX_FLAGS "-D_LANGUAGE_C_PLUS_PLUS ${CMAKE_CXX_FLAGS}")

SET(CMAKE_Fortran_IMPLICIT_LINK_LIBRARIES "m;c")

INCLUDE(includes.cmake)

# This kills the libpathfortran build at src/libu/numconv/mpp/defgu2s.c
#ADD_DEFINITIONS(-D_GNU_SOURCE)

# Fake target that depends on all be components
add_custom_target(be-all)

ADD_SUBDIRECTORY(csu)
ADD_SUBDIRECTORY(driver)
ADD_SUBDIRECTORY(libcif)
ADD_SUBDIRECTORY(libcmplrs)
ADD_SUBDIRECTORY(libdwarf)
ADD_SUBDIRECTORY(libelf)
ADD_SUBDIRECTORY(libelfutil)
ADD_SUBDIRECTORY(common/rtkutils)
ADD_SUBDIRECTORY(common/targ_info/parser/gensyn)
ADD_SUBDIRECTORY(common/targ_info)
ADD_SUBDIRECTORY(common/util)
ADD_SUBDIRECTORY(libSYS)
ADD_SUBDIRECTORY(be)
ADD_SUBDIRECTORY(libpscrt)
path64_is_arch_enabled(arm_enabled arm)
if(${arm_enabled})
ADD_SUBDIRECTORY(libfereconf)
endif()
ADD_SUBDIRECTORY(wgen)
ADD_SUBDIRECTORY(ipa)
ADD_SUBDIRECTORY(ipa_driver)
# TODO : Threading and other things need to be ported
IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
ADD_SUBDIRECTORY(libopenmp)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
ADD_SUBDIRECTORY(libfi)

# All the math stuff seems to be built from libmpath
# libmv == vector math functions
ADD_SUBDIRECTORY(libm)
ADD_SUBDIRECTORY(libmpath)

ADD_SUBDIRECTORY(instrumentation)
ADD_SUBDIRECTORY(arith)
# TODO : Can't determine linking method

IF(${PATH64_ENABLE_FORTRAN})
ADD_SUBDIRECTORY(libpathfortran)
ADD_SUBDIRECTORY(crayf90)
IF(${PATH64_ENABLE_MATHLIBS})
ADD_SUBDIRECTORY(f95_coco)
ENDIF()
ENDIF(${PATH64_ENABLE_FORTRAN})

ADD_SUBDIRECTORY(ir_tools)
ADD_SUBDIRECTORY(tools)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
# This is for huge pages on linux
     ADD_SUBDIRECTORY(libhugetlbfs)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
# TODO : Pull in code for superpages on FBSD
# TODO : Pull in integrate with MPSS (Multiple Page Size Support) on Solaris
# http://hub.opensolaris.org/bin/download/Project+muskoka/virtual_memory/opensolarisvm1.pdf
# http://docs.sun.com/app/docs/doc/816-0210/6m6nb7mf1?a=view
# http://cvs.opensolaris.org/source/xref/onnv/onnv-gate/usr/src/uts/sun4/os/startup.c
# 
