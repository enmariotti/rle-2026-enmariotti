#!/usr/bin/env bash
set -e

for f in *.bmp; do ./rle --encode "$f" "${f%.bmp}.prle" ; done

sleep 2

for f in *.prle; do ./rle --decode "$f" "${f%.prle}_decoded.bmp" ; done
