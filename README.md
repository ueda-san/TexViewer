# TexViewer
This is an image viewer that excels at handling textures and HDR images.
It also includes basic editing features like cropping and resizing images.
<p align="right">→<a href="README_ja.md">日本語</a></p>

<p><a href="https://apps.microsoft.com/detail/9n497rjhvx9z?referrer=appbadge&mode=direct"><img src="https://get.microsoft.com/images/en-us%20dark.svg" width="200"/></a></p>

## Key Features

<img width="193" height="222" alt="main_en" src="https://github.com/user-attachments/assets/aa206436-dbc2-4bb0-a9cf-0c5c06e0bd5f" />
<img width="208" height="223" alt="plugin_en" src="https://github.com/user-attachments/assets/9edd50ba-a0a7-41f3-847f-a49368a14777" /><br/>
* Supports various texture formats such as PVR and ASTC.
* Supports HDR formats such as AVIF and UltraHDR.
* It can display colors correctly using color space information from textures or profiles like CICP and ICC.
* It can also read over 100 image formats supported by ImageMagick.
* It supports alpha-channel images, displaying transparent areas as a checkerboard pattern.
* It enables blur-free zooming and pixel-perfect area selection.
* Animated images like GIF, PNG, and HEIF can be displayed with animation.
* RGB(A) arrays without headers can also be read.
* It is not suitable for viewing HDR images on an SDR monitor.

## How to use

→ [How to use TexViewer](https://github.com/ueda-san/TexViewer/wiki/How-to-use-%E2%80%90-en)

## Supported file extensions

|Extension |Description|
| --- | --- |
|astc      |Adaptive Scalable Texture Compression          |
|avif      |AV1 Image File Format                          |
|basis     |Basis Universal GPU texture                    |
|dds       |Direct Draw Surface                            |
|dng       |Digital Negative                               |
|exr       |OpenEXR image                                  |
|gif       |CompuServe Graphics Interchange Format         |
|hdp(wdp)  |Windows Media Photo                            |
|hdr       |Radiance RGBE image format                     |
|heif(heic)|Apple High Efficiency Image Format             |
|ico       |Microsoft icon                                 |
|jpeg      |Joint Photographic Experts Group JFIF format   |
|jxr       |Joint Photographic Experts Group Extended Range|
|ktx(ktx2) |Khronos Texture                                |
|pkm       |Ericsson texture compression                   |
|png       |Portable Network Graphics                      |
|psd       |Adobe Photoshop multispectral bitmap           |
|pvr       |PowerVR texture compression                    |
|tga       |Truevision Graphics Adapter                    |
|tiff      |Tagged Image File Format                       |
|webp      |Weppy image format                             |

Additionally, ImageMagick supports over 100 image formats.
