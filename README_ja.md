# TexViewer

テクスチャやHDR画像に強い系の画像ビューアです。
画像の切り出しやモザイクなど簡単な編集機能もあります。
<p align="right">→<a href="README.md">English</a></p>

<p><a href="https://apps.microsoft.com/detail/9n497rjhvx9z?referrer=appbadge&mode=direct"><img src="https://get.microsoft.com/images/ja%20dark.svg" width="200"/></a></p>


## 主な特徴

<img width="193" height="222" alt="main_ja" src="https://github.com/user-attachments/assets/981b1f63-e7a8-402b-9d3e-f5788cff466f" /><br/>
* PVR や ASTC など各種テクスチャ形式に対応しています。
* AVIF や UltraHDR など HDR 形式に対応しています。
* テクスチャの色域情報または CICP や ICC などから正しい色で表示できます。
* ImageMagick が対応している100以上の画像形式も読み込めます。
* アルファ付き画像対応、透明部分はチェッカーボード柄で表示します。
* ボケない拡大&ピクセルパーフェクトな範囲選択が可能です。
* gif, png, heif などアニメーション可能な画像はアニメーション表示できます。
* header 無しの RGB(A) 配列も読み込めます。
* HDR 画像を SDR モニタで見る用途には向いていません。


## 使い方

→ [TexViewer の使い方](https://github.com/ueda-san/TexViewer/wiki/How-to-use-%E2%80%90-ja)

## 対応している拡張子

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

その他、ImageMagick が対応している 100 以上の画像形式
