{
  "targets": [
    {
      "target_name": "atom_patch",
      "cflags_cc": ["-std=c++11"],
      "sources": [
        "src/binding.cc",
        "src/patch.cc",
        "src/point.cc",
      ],
      "include_dirs": [
        "src",
        '<!(node -e "require(\'nan\')")'
      ],
      "conditions": [
        ['OS=="mac"', {
          "xcode_settings": {
              "cflags_cc": ["-std=c++11", "-stdlib=libc++"],
              'OTHER_CPLUSPLUSFLAGS' : ['-std=c++11','-stdlib=libc++'],
              'OTHER_LDFLAGS': ['-stdlib=libc++'],
              'MACOSX_DEPLOYMENT_TARGET': '10.9'
          }
        }]
      ]
    },
  ],

  "variables": {
    "tests": 0
  },

  "conditions": [
    ['tests != 0', {
      "targets": [{
        "target_name": "tests",
        "type": "executable",
        "cflags_cc": ["-std=c++11", "-fexceptions"],
        "sources": [
          "src/patch.cc",
          "src/point.cc",
          "test/patch_test.cc",
          "test/tests.cc",
        ],
        "include_dirs": [
          "src",
          "test",
          "vendor",
          '<!(node -e "require(\'nan\')")'
        ],
        "conditions": [
          ['OS=="mac"', {
            "xcode_settings": {
                "cflags_cc": ["-std=c++11", "-stdlib=libc++"],
                'OTHER_CPLUSPLUSFLAGS' : ['-std=c++11','-stdlib=libc++'],
                'OTHER_LDFLAGS': ['-stdlib=libc++'],
                'MACOSX_DEPLOYMENT_TARGET': '10.9',
                'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
            }
          }]
        ]
      }]
    }]
  ]
}
