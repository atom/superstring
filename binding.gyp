{
  "targets": [
    {
      "target_name": "marker_index",
      "cflags_cc": ["-std=c++11"],
      "sources": [
        "src/native/iterator.cc",
        "src/native/marker-index-wrapper.cc",
        "src/native/marker-index.cc",
        "src/native/node.cc",
        "src/native/point.cc",
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
              'MACOSX_DEPLOYMENT_TARGET': '10.7'
          }
        }]
      ]
    }
  ]
}
