#!/usr/bin/env bash

./progvid $* | \
    ffmpeg -f image2pipe -framerate 60 -c:v ppm -i - -c:v libx264 -pix_fmt yuv420p output.mp4
