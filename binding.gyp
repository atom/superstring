{
  "targets": [
    {
      "target_name": "atom_patch",
      "sources": [
        "src/binding.cc",
      ]
    },
  ],

  "variables": {
    "tests": 0
  },

  "conditions": [
    # If --tests is passed to node-gyp configure, we'll build a standalone
    # executable that runs tests on the patch.
    ['tests != 0', {
      "targets": [{
        "target_name": "tests",
        "type": "executable",
        "cflags_cc": ["-fexceptions"],
        "sources": [
          "test/patch_test.cc",
          "test/tests.cc",
        ],
        "include_dirs": [
          "test",
          "vendor",
        ],
        "conditions": [
          ['OS=="mac"', {
            "xcode_settings": {
              "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
            }
          }],
          # Enable debugger-friendly optimizations on Linux so we don't retain
          # v8 symbols that won't be defined when linking the tests executable.
          ['OS=="linux"', {
            'configurations': {
              'Debug': {
                'cflags!': [ '-O0' ],
                'cflags': [ '-Og' ]
              }
            }
          }],
        ]
      }]
    }]
  ],

  "target_defaults": {
    "cflags_cc": ["-std=c++11"],
    "sources": [
      "src/patch.cc",
      "src/point.cc",
      "src/text.cc",
    ],
    "include_dirs": [
      "src",
      "<!(node -e \"require('nan')\")"
    ],
    "conditions": [
      ['OS=="mac"', {
        "xcode_settings": {
          "OTHER_CPLUSPLUSFLAGS" : ["-std=c++11", "-stdlib=libc++"],
          "OTHER_LDFLAGS": ["-stdlib=libc++"],
          "MACOSX_DEPLOYMENT_TARGET": "10.9",
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
