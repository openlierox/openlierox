#!/bin/bash

mkdir -p bin
g++ -Wall -Wno-sign-compare src/*.cpp -o bin/udpmasterserver -O3
