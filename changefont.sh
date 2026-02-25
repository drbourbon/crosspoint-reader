#!/bin/bash
TARGET=lib/EpdFont/builtinFonts/

if [ "$#" -ne 2 ]; then
    echo "Reverting to default builtin fonts"
    git restore -- ${TARGET}
#    cp -v fonts/save/*.h ${TARGET}
else

TARGET_BASE=bookerly
#TARGET_BASE=notosans
#TARGET_BASE=opendyslexic
SIZE_GAP=$1
FONT_STD=$2
for size in 12 14 16 18 ; do
  echo "echo processing size ${size}"
  for type in regular bold italic bolditalic ; do
    Type="$(tr '[:lower:]' '[:upper:]' <<< ${type:0:1})${type:1}"
    FileName=${FONT_STD}${Type}.ttf
    if ! test -f ${FileName}; then
      FileName=${FONT_STD}${Type}.otf
    fi
    echo python3 lib/EpdFont/scripts/fontconvert.py  ${TARGET_BASE}_${size}_${type} $(( size + SIZE_GAP )) "${FileName}" --2bit --compress">" ${TARGET}${TARGET_BASE}_${size}_${type}.h 
  done
done

fi

#ls -l ${TARGET}
