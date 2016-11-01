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
    }
  ]
}
