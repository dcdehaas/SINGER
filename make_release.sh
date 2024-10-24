#!/bin/sh
set -ev

VER=0.1.8
REL_DIR=releases/singer-${VER}-beta-linux-x86_64

cp build/singer ${REL_DIR}
cp SINGER/SINGER/singer_master ${REL_DIR}
cp SINGER/SINGER/parallel_singer ${REL_DIR}
cp SINGER/SINGER/convert_to_tskit ${REL_DIR}
cp SINGER/SINGER/convert_long_ARG.py ${REL_DIR}

tar -czf singer-${VER}-drews-linux-x86_64.tar.gz ${REL_DIR}
