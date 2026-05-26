#!/usr/bin/env python3

import os
import sys

os.chdir(os.path.dirname(sys.argv[0]))

os.system("./fixer.py /dev/stdin")
