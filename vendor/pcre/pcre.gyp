{
    "targets": [
        {
            "target_name": "pcre",
            "type": "static_library",
            "sources": [
                "pcre2_chartables.c",
                "10.23/src/pcre2_auto_possess.c",
                "10.23/src/pcre2_compile.c",
                "10.23/src/pcre2_config.c",
                "10.23/src/pcre2_context.c",
                "10.23/src/pcre2_dfa_match.c",
                "10.23/src/pcre2_error.c",
                "10.23/src/pcre2_find_bracket.c",
                "10.23/src/pcre2_jit_compile.c",
                "10.23/src/pcre2_maketables.c",
                "10.23/src/pcre2_match.c",
                "10.23/src/pcre2_match_data.c",
                "10.23/src/pcre2_newline.c",
                "10.23/src/pcre2_ord2utf.c",
                "10.23/src/pcre2_pattern_info.c",
                "10.23/src/pcre2_serialize.c",
                "10.23/src/pcre2_string_utils.c",
                "10.23/src/pcre2_study.c",
                "10.23/src/pcre2_substitute.c",
                "10.23/src/pcre2_substring.c",
                "10.23/src/pcre2_tables.c",
                "10.23/src/pcre2_ucd.c",
                "10.23/src/pcre2_valid_utf.c",
                "10.23/src/pcre2_xclass.c",
            ],
            "include_dirs": [
                "include",
                "10.23/src"
            ],
            "defines": [
                "HAVE_CONFIG_H",
                "PCRE2_CODE_UNIT_WIDTH=16",
            ],
            "cflags": [
                "-Wno-unused-function"
            ],
            'xcode_settings': {
                'OTHER_CFLAGS': [
                    '-Wno-unused-function'
                ],
            },
            "direct_dependent_settings": {
                "include_dirs": [
                    "include"
                ],
                "defines": [
                    "PCRE2_CODE_UNIT_WIDTH=16",
                ]
            }
        }
    ]
}