{
    "targets": [
        {
            "target_name": "superstring",
            "dependencies": [
                "superstring_core"
            ],
            "sources": [
                "src/bindings/bindings.cc",
                "src/bindings/marker-index-wrapper.cc",
                "src/bindings/patch-wrapper.cc",
                "src/bindings/point-wrapper.cc",
                "src/bindings/range-wrapper.cc",
                "src/bindings/text-buffer-wrapper.cc",
                "src/bindings/text-buffer-snapshot-wrapper.cc",
                "src/bindings/text-reader.cc",
                "src/bindings/string-conversion.cc",
                "src/bindings/text-writer.cc",
            ],
            "include_dirs": [
              "src/core",
              "<!(node -e \"require('nan')\")"
            ],
        },
        {
            "target_name": "superstring_core",
            "type": "static_library",
            "dependencies": [
                "./vendor/pcre/pcre.gyp:pcre",
            ],
            "sources": [
                "src/core/encoding-conversion.cc",
                "src/core/marker-index.cc",
                "src/core/patch.cc",
                "src/core/point.cc",
                "src/core/range.cc",
                "src/core/regex.cc",
                "src/core/text.cc",
                "src/core/text-buffer.cc",
                "src/core/text-slice.cc",
                "src/core/text-diff.cc",
                "src/core/libmba-diff.cc",
            ],
            "include_dirs": [
                "vendor/libcxx"
            ],
            "conditions": [
                ['OS=="mac"', {
                    'link_settings': {
                        'libraries': ['libiconv.dylib'],
                    }
                }],
                ['OS=="win"', {
                   'sources': [
                       'vendor/win-iconv/win_iconv.c',
                    ],
                    'include_dirs': [
                        'vendor/win-iconv'
                    ],
                    'defines': [
                        'WINICONV_CONST=',
                        'PCRE2_STATIC',
                    ]
                }],
            ],
        }
    ],

    "variables": {
        "tests": 0,
        "STANDARD": 17,
        "MACOSX_DEPLOYMENT_TARGET": "10.15"
    },

    "conditions": [
        # If --tests is passed to node-gyp configure, we'll build a standalone
        # executable that runs tests on the patch.
        ['tests != 0', {
            "targets": [{
                "target_name": "tests",
                "type": "executable",
                "cflags_cc!": ["-fno-exceptions"],
                "defines": [
                    "CATCH_CONFIG_CPP11_NO_IS_ENUM"
                ],
                "sources": [
                    "test/native/test-helpers.cc",
                    "test/native/tests.cc",
                    "test/native/encoding-conversion-test.cc",
                    "test/native/patch-test.cc",
                    "test/native/text-buffer-test.cc",
                    "test/native/text-test.cc",
                    "test/native/text-diff-test.cc",
                ],
                "include_dirs": [
                    "vendor",
                    "src/core",
                ],
                "dependencies": [
                    "superstring_core"
                ],
                "conditions": [
                    ['OS=="mac"', {
                        'cflags': [
                            "-mmacosx-version-min=<(MACOSX_DEPLOYMENT_TARGET)"
                        ],
                        "xcode_settings": {
                            "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
                            'MACOSX_DEPLOYMENT_TARGET': '<(MACOSX_DEPLOYMENT_TARGET)',
                        }
                    }]
                ]
            }]
        }]
    ],
    "target_defaults": {
        "cflags_cc": [ "-std=c++<(STANDARD)" ],
        "conditions": [
            ['OS=="mac"', {
                "xcode_settings": {
                    'CLANG_CXX_LIBRARY': 'libc++',
                    'CLANG_CXX_LANGUAGE_STANDARD':'c++(STANDARD)',
                }
            }],
            ['OS=="win"', {
                "link_settings": {
                    "libraries": ["ws2_32.lib"]
                },
                "defines": [
                    "NOMINMAX"
                ],
            }]
        ],
        'default_configuration': 'Release',
        'configurations': {
          # Release Settings
          'Release': {
            'defines': [ 'NDEBUG' ],
            "cflags": [ "-fno-exceptions", "-O3" ],
            "cflags_cc": [ "-fno-exceptions", "-O3", "-std=c++<(STANDARD)" ],
            "xcode_settings": {
              'GCC_OPTIMIZATION_LEVEL': '3', # stop gyp from defaulting to -Os
              "CLANG_CXX_LIBRARY": "libc++",
              "CLANG_CXX_LANGUAGE_STANDARD": "c++<(STANDARD)",
              'MACOSX_DEPLOYMENT_TARGET': "<(MACOSX_DEPLOYMENT_TARGET)"
            }, # XCODE
            "msvs_settings": {
              "VCCLCompilerTool": {
                'ExceptionHandling': 0,               # /EHsc
                'MultiProcessorCompilation': 'true',
                'RuntimeTypeInfo': 'false',
                'Optimization': 3,              # full optimizations /O2  ==  /Og /Oi /Ot /Oy /Ob2 /GF /Gy
                'StringPooling': 'true',        # pool string literals
                "AdditionalOptions": [
                  # C++ standard
                  "/std:c++<(STANDARD)",

                  # Optimizations
                  "/O2",
                  # "/Ob3",  # aggressive inline
                  "/GL",   # whole Program Optimization  # /LTCG is implied with /GL.
                  "/DNDEBUG"  # turn off asserts
                ],
              }
            } # MSVC
          }, # Release
        }, # configurations
    } # target-defaults
}
