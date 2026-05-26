#!/usr/bin/env python3

# Liero Xtreme utilities for Python
# Code released under the LGPL license
# Created on 18/02/2009 by Karel Petranek
# Ported to Python on 04/02/2010 by Albert Zeyer

# Requirements:
# Pillow (PIL fork) and zlib modules

# This file contains functions for working with (Open)LieroX levels (*.lxl)

from PIL import Image
import zlib


def ReadFixedCStr(f, num):
	data = f.read(num)
	for i in range(num):
		if data[i] == 0:
			data = data[0:i]
			break
	return data.decode("latin-1")


def ReadInt32LE(f):
	d = f.read(4)
	return d[0] + d[1] * 256 + d[2] * 256 * 256 + d[3] * 256 * 256 * 256


class LXLevel:
	Name = ""
	Width = 0
	Height = 0
	Front = None
	Back = None
	Mat = None

	def load(self, fn):
		f = open(fn, 'rb')

		# Header
		id = ReadFixedCStr(f, 32)
		version = ReadInt32LE(f)

		# Check the header
		if ((id != "LieroX Level" and id != "LieroX CTF Level") or version != 0):
			print("Wrong file format", id, version)
			return False

		self.Name = ReadFixedCStr(f, 64)
		print("Map name:", self.Name)

		# Dimensions
		self.Width = ReadInt32LE(f)
		self.Height = ReadInt32LE(f)
		print("Size:", str(self.Width) + "x" + str(self.Height))

		# Type
		type = ReadInt32LE(f)
		LXL_IMAGE = 1
		if type != LXL_IMAGE:
			print("Must be LXL_IMAGE type")
			return False

		# Theme
		theme = ReadFixedCStr(f, 32)
		num_objects = ReadInt32LE(f)

		# Compressed data info
		packed_size = ReadInt32LE(f)
		unpacked_size = ReadInt32LE(f)

		# Read the compressed data
		compressed = f.read(packed_size)
		uncompressed = zlib.decompress(compressed)
		if not uncompressed:
			return False
		compressed = None  # Save some memory

		# Create the level image
		self.Front = Image.new("RGB", (self.Width, self.Height))
		self.Back = Image.new("RGB", (self.Width, self.Height))
		self.Mat = Image.new("P", (self.Width, self.Height))

		palette = []
		palette.extend((0, 0, 0))        # 0 - rock
		palette.extend((255, 255, 255))  # 1 - background
		palette.extend((255, 0, 0))      # 2 - dirt
		for i in range(256 - 3):
			palette.extend((i, i, i))    # grayscale wedge

		self.Mat.putpalette(palette)
		frontTransp = (255, 0, 255)

		front = self.Front.load()
		back = self.Back.load()
		mat = self.Mat.load()

		PX_EMPTY = 1
		PX_DIRT = 2
		PX_ROCK = 4
		bi = 0  # Position in uncompressed buffer, beginning of background image
		fi = self.Width * self.Height * 3  # Beginning of foreground image (uncompressed)
		mi = fi + self.Width * self.Height * 3  # Beginning of material (uncompressed)
		for y in range(self.Height):
			for x in range(self.Width):
				backCol = (
					uncompressed[bi],
					uncompressed[bi + 1],
					uncompressed[bi + 2])
				frontCol = (
					uncompressed[fi],
					uncompressed[fi + 1],
					uncompressed[fi + 2])

				front[x, y] = frontCol
				back[x, y] = backCol

				if uncompressed[mi] & PX_EMPTY:
					front[x, y] = backCol
					#front[x,y] = frontTransp # only makes sense at all with paralax but even then, better the user edits it
					mat[x, y] = 1  # background
				elif uncompressed[mi] & PX_ROCK:
					mat[x, y] = 0  # rock
				else:
					mat[x, y] = 2  # dirt

				bi += 3
				fi += 3
				mi += 1

		return True
