#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import os.path
import subprocess
path = os.getcwd()

excluded_directories = ["pybind11", "/openssl", "/zlib", "/build/generated/", "cmake-build-debug/", "cmake-build-release", "CMakeFiles"]
for root, dirs, files in os.walk(path):
    if any(excluded_dir in root for excluded_dir in excluded_directories):
        continue
    for name in files:
        if name.endswith((".h", ".m", ".mm", ".hpp", ".cpp", ".java")):
            localpath = os.path.join(root, name)
            print(localpath)
            subprocess.run(["clang-format", "-i", localpath, "-style=File"], check=True)
