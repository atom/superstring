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
                "src/bindings/buffer-offset-index-wrapper.cc",
            ],
            "include_dirs": [
              "src/core",
              "<!(node -e \"require('nan')\")"
            ],
            'link_settings': {
                'libraries': [
                  '/usr/local/lib/libprofiler.a',
                ]
            }
        },
        {
            "target_name": "superstring_core",
            "type": "static_library",
            "sources": [
                "src/core/patch.cc",
                "src/core/point.cc",
                "src/core/range.cc",
                "src/core/text.cc",
                "src/core/marker-index.cc",
                "src/core/buffer-offset-index.cc"
            ]
        },
    ],

    "variables": {
        "tests": 0,
        "benchmarks": 0
    },

    "conditions": [
        # If --tests is passed to node-gyp configure, we'll build a standalone
        # tests executable.
        ['tests != 0', {
            "targets": [{
                "target_name": "tests",
                "type": "executable",
                "cflags_cc": ["-fexceptions"],
                "sources": [
                    "test/native/patch-test.cc",
                    "test/native/tests.cc",
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
                        "xcode_settings": {
                            "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
                        }
                    }]
                ]
            }]
        }],
        # If --benchmarks is passed to node-gyp configure, we'll build a standalone
        # benchmarks executable.
        ['benchmarks != 0', {
            "targets": [{
                "target_name": "benchmarks",
                "type": "executable",
                "cflags_cc": ["-fexceptions"],
                "sources": [
                    "benchmark/native/marker-index-benchmark.cc",
                    "benchmark/native/benchmarks.cc",
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
                        "xcode_settings": {
                            "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
                        }
                    }]
                ]
            }]
        }]
    ],

    "target_defaults": {
        "cflags_cc": ["-std=c++11"],
        "conditions": [
            ['OS=="mac"', {
                "xcode_settings": {
                    'CLANG_CXX_LIBRARY': 'libc++',
                    'CLANG_CXX_LANGUAGE_STANDARD':'c++11',
                }
            }],
            ['OS=="win"', {
                "link_settings": {
                    "libraries": ["ws2_32.lib"]
                }
            }]
        ]
    }
}
