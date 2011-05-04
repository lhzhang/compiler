PROJECT(libdwarf)

INCLUDE_DIRECTORIES(BEFORE
  ${PATHSCALE_INCLUDE_DIR}/libelf/lib
  ${libdwarf_SOURCE_DIR}/libdwarf
  ${libdwarf_BINARY_DIR}
  )

add_custom_command(
OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pubnames.c
COMMAND m4 -D SRCDIR=${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_nametbl.m4 ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pubnames.m4 > ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pubnames.c
)
add_custom_command(
OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pubtypes.c
COMMAND m4 -D SRCDIR=${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_nametbl.m4 ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pubtypes.m4 > ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pubtypes.c
)
add_custom_command(
OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_weaks.c
COMMAND m4 -D SRCDIR=${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_nametbl.m4 ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_weaks.m4 > ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_weaks.c
)
add_custom_command(
OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_funcs.c
COMMAND m4 -D SRCDIR=${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_nametbl.m4 ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_funcs.m4 > ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_funcs.c
)
add_custom_command(
OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_vars.c
COMMAND m4 -D SRCDIR=${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_nametbl.m4 ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_vars.m4 > ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_vars.c
)
add_custom_command(
OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_types.c
COMMAND m4 -D SRCDIR=${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_nametbl.m4 ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_types.m4 > ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_types.c
)


add_custom_command(
OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pro_pubnames.c
COMMAND m4 -D SRCDIR=${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pro_nametbl.m4 ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pro_pubnames.m4 > ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pro_pubnames.c
)
add_custom_command(
OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pro_weaks.c
COMMAND m4 -D SRCDIR=${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pro_nametbl.m4 ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pro_weaks.m4 > ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pro_weaks.c
)
add_custom_command(
OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pro_funcs.c
COMMAND m4 -D SRCDIR=${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pro_nametbl.m4 ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pro_funcs.m4 > ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pro_funcs.c
)
add_custom_command(
OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pro_vars.c
COMMAND m4 -D SRCDIR=${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pro_nametbl.m4 ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pro_vars.m4 > ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pro_vars.c
)
add_custom_command(
OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pro_types.c
COMMAND m4 -D SRCDIR=${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pro_nametbl.m4 ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pro_types.m4 > ${CMAKE_CURRENT_SOURCE_DIR}/dwarf_pro_types.c
)



path64_set_multiarch_sources(dwarf_sources COMMON
	libdwarf/dwarf_abbrev.c
	libdwarf/dwarf_arange.c
	libdwarf/dwarf_attr.c
	libdwarf/dwarf_attrval.c
	libdwarf/dwarf_cu.c
	libdwarf/dwarf_dealloc.c
	libdwarf/dwarf_die.c
	libdwarf/dwarf_dump.c
	libdwarf/dwarf_errmsg.c
	libdwarf/dwarf_finish.c
	libdwarf/dwarf_form.c
	libdwarf/dwarf_frame.c
	libdwarf/dwarf_funcs.c
	libdwarf/dwarf_init.c
	libdwarf/dwarf_lineno.c
	libdwarf/dwarf_loclist.c
	libdwarf/dwarf_macinfo.c
	libdwarf/dwarf_pro_arange.c
	libdwarf/dwarf_pro_attr.c
	libdwarf/dwarf_pro_die.c
	libdwarf/dwarf_pro_expr.c
	libdwarf/dwarf_pro_finish.c
	libdwarf/dwarf_pro_frame.c
	libdwarf/dwarf_pro_funcs.c
	libdwarf/dwarf_pro_init.c
	libdwarf/dwarf_pro_lineno.c
	libdwarf/dwarf_pro_macinfo.c
	libdwarf/dwarf_pro_pubnames.c
	libdwarf/dwarf_pro_reloc.c
	libdwarf/dwarf_pro_sections.c
	libdwarf/dwarf_pro_types.c
	libdwarf/dwarf_pro_vars.c
	libdwarf/dwarf_pro_weaks.c
	libdwarf/dwarf_pubnames.c
	libdwarf/dwarf_pubtypes.c
	libdwarf/dwarf_ranges.c
	libdwarf/dwarf_reloc.c
	libdwarf/dwarf_seterror.c
	libdwarf/dwarf_str.c
	libdwarf/dwarf_types.c
	libdwarf/dwarf_vars.c
	libdwarf/dwarf_weaks.c
	libdwarf/libdwarf.c
	libdwarf/libdwarf_abbrev.c
	libdwarf/libdwarf_arange.c
	libdwarf/libdwarf_attr.c
	libdwarf/libdwarf_die.c
	libdwarf/libdwarf_error.c
	libdwarf/libdwarf_elf_access.c
	libdwarf/libdwarf_elf_init.c
	libdwarf/libdwarf_frame.c
	libdwarf/libdwarf_info.c
	libdwarf/libdwarf_init.c
	libdwarf/libdwarf_lineno.c
	libdwarf/libdwarf_loc.c
	libdwarf/libdwarf_loclist.c
	libdwarf/libdwarf_macinfo.c
	libdwarf/libdwarf_nametbl.c
	libdwarf/libdwarf_ranges.c
	libdwarf/libdwarf_reloc.c
	libdwarf/libdwarf_rw.c
	libdwarf/libdwarf_sections.c
	libdwarf/libdwarf_str.c
        )
path64_add_multiarch_library(dwarf STATIC dwarf_sources)
