{
  "targets": [
    {
      "target_name": "windows_process_tree",
      "product_extension": "node",
      "sources": [
        "src/addon.cc",
        "src/cpu_worker.cc",
        "src/process.cc",
        "src/process_worker.cc",
        "src/process_commandline.cc"
      ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")"
      ],
      "libraries": [ 'psapi.lib' ]
    },
    {
      "target_name": "action_after_build",
      "type": "none",
      "dependencies": [ "<(module_name)" ],
      "copies":
      [
        {
            "files": [ "<(PRODUCT_DIR)/<(module_name).node" ],
            "destination": "<(module_path)"
        }
      ]
    }
  ]
}
