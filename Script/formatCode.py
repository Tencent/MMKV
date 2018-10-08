#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import os.path
path = os.getcwd()

for root, dirs, files in os.walk(path):
    if root.endswith("/aes") or root.endswith("/openssl"):
        continue
    for name in files:
        if (name.endswith(".h") or name.endswith(".m") or name.endswith(".mm") or name.endswith(".hpp") or name.endswith(".cpp")):
            localpath = root + '/' + name
            print localpath
            os.system("clang-format -i %s -style=File" %(localpath))
