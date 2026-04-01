#!/bin/sh

# blank-file-6-256.png
# https://www.iconsdb.com/gray-icons/blank-file-6-icon.html
# CC0 1.0 Universal (CC0 1.0) Public Domain Dedication.


function drawIcon(){
    magick blank-file-6-256.png -fill $2 -draw "rectangle 5,140 220,230" -fill "#010101" -gravity Center -font Tahoma-Bold -pointsize 80 -annotate -12+55 $1 ../TexViewer/Assets/Icons/$1.png
    #sleep 1 # Might be needed to work around weird size in file headers with very old ImageMagick?
}


rm ../TexViewer/Assets/Icons/*

# general for images without icons
drawIcon img "#808080"

# from Associated colors(?)
drawIcon astc  "#008fbe"  # Arm
drawIcon pvr   "#a54f8d"  # Imagination Technology
drawIcon ktx   "#00e0c0"
drawIcon ktx2  "#00c0e0"  # etc,uastc
drawIcon basis "#80a080"
drawIcon dds   "#aece17"  # DirectX
drawIcon psd   "#eb1000"  # Adobe
drawIcon pdf   "#eb2000"  # Adobe
drawIcon ai    "#eb3000"  # Adobe
drawIcon webp  "#58a60d"  # logo
drawIcon heif  "#ff80c0"  # logo
drawIcon exr   "#ca2e3c"  # OpenEXR
drawIcon xcf   "#8d8369"  # Gimp
drawIcon pkm   "#5078ae"  # Ericsson
drawIcon ico   "#008080"  # VS Icon Editor
drawIcon gif   "#6f67c6"  # CompuServe
drawIcon avif  "#fbac30"  # logo
drawIcon miff  "#ffff00"  # Magic wand

# Frequently used extensions
drawIcon png  "#50ff00"
drawIcon mng  "#50ff50"
drawIcon apng "#50ffa0"

drawIcon jpeg "#40a0ff"
drawIcon jp2  "#4080ff"
drawIcon jxr  "#ff40c0"
drawIcon hdp  "#ff20a0"
drawIcon dng  "#4060ff"

drawIcon bmp "#ff6040"

drawIcon tga  "#ffc000"
drawIcon tiff "#ff8000"
drawIcon hdr  "#ff4080"
drawIcon svg  "#00c020"
