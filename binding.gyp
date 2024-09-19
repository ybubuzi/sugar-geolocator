{
   
  "targets": [
    {
      "target_name": "sugar_geolocator",
      "sources": ["src/addon.cpp"],
      "include_dirs": ["<!@(node -p \"require('node-addon-api').include\")"],
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "defines": ["NAPI_DISABLE_CPP_EXCEPTIONS"],
      "conditions": [
        [
          "OS=='mac'",
          {
            "xcode_settings": {
              "OTHER_CPLUSPLUSFLAGS": ["-stdlib=libc++"]
            }
          }
        ]
      ]
    }
  ]
}
