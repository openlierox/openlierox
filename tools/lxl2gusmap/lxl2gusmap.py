#!/usr/bin/env python3

from lx_level import *
import sys
import os

level = LXLevel()
if not level.load(sys.argv[1]):
	exit

if not os.path.exists(level.Name): os.mkdir(level.Name)

level.Front.save(level.Name + "/level.png")
level.Back.save(level.Name + "/background.png")
level.Mat.save(level.Name + "/material.png")

open(level.Name + "/config.cfg", "w").close()
