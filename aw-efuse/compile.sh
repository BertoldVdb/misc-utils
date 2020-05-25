#!/bin/sh

arm-linux-musleabi-gcc -static -Os main.c -o efuse && arm-linux-musleabi-strip efuse
