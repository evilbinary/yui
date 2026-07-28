# coding:utf-8
target("tsm")
set_kind("static")
add_flags()
add_includedirs('.', 'src', 'external')
add_files(
    'src/tsm_screen.c',
    'src/tsm_vte.c',
    'src/tsm_vte_charsets.c',
    'src/tsm_unicode.c',
    'src/shl_htable.c',
    'external/wcwidth.c',
)
