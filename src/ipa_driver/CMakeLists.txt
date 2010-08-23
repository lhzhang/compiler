
set(IPA_DRIVER_SRCS
"ipa_driver_main.cxx"
"ipa_command_line.cxx"
"ipa_symbols_sync.cxx"
)


INCLUDE_DIRECTORIES(
                                       ${PATHSCALE_BINARY_DIR}/common/targ_info				
                                 	../common/com
					../common/com/@ARCH@
					../common/util
					../ipa/common
					../ipa/local
					../ipa/main/analyze
					../ipa/main/optimize
					../be/com
					)

ADD_DEFINITIONS(${PATH64_STDC_FORMAT_MACROS})

set_property(SOURCE ${IPA_DRIVER_SRCS} PROPERTY COMPILE_DEFINITIONS __MIPS_AND_IA64_ELF_H;MONGOOSE_BE)

IF(CMAKE_BUILD_TYPE STREQUAL "Debug")

path64_set_multiarch_sources(ipa_driver_sources COMMON ${IPA_DRIVER_SRCS})
path64_add_multiarch_executable(ipa_driver ipa_driver_sources)
path64_multiarch_link_libraries(ipa_driver ${libdl})

ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")
