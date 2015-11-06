{
  "targets": [
    {
      "target_name": "marker_index",
      "cflags_cc": ["-std=c++11", "-stdlib=libc++"],
      "sources": [
        "src/iterator.cc",
        "src/marker-index-wrapper.cc",
        "src/marker-index.cc",
        "src/node.cc",
        "src/point.cc",
      ],
      "include_dirs": [
        "src",
        '<!(node -e "require(\'nan\')")'
      ],
      "conditions": [
        ['OS=="mac"', {
          "xcode_settings": {
              'OTHER_CPLUSPLUSFLAGS' : ['-std=c++11','-stdlib=libc++'],
              'OTHER_LDFLAGS': ['-stdlib=libc++'],
              'MACOSX_DEPLOYMENT_TARGET': '10.7'
          }
        }]
      ]
    }
  ]
}
