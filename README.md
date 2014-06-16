**RGBX**

rgbx is a tool to try different encoding hdr to rgba8 format
it supports:
- rgbe
- rgbm
- rgbd
- rgbd2 ( rgbd with range )

**usage**

    // encode
    rgbx -m method [-r range] inputImage_hdr.tif outputImage_rgba8.png
    
    // decode
    rgbx -d -m method [-r range] inputImage_rgba8.png outputImage_hdr.tif

**dependancies**
- openimageio
- cmake

**builds**

    mkdir build
    cmake ../
    make


**Links**
- http://iwasbeingirony.blogspot.fr/2010/06/difference-between-rgbm-and-rgbd.html
- http://graphicrants.blogspot.fr/2009/04/rgbm-color-encoding.html
- https://gist.github.com/aras-p/1199797
- http://mynameismjp.wordpress.com/2008/12/12/logluv-encoding-for-hdr/
- http://lousodrome.net/blog/light/2013/05/26/gamma-correct-and-hdr-rendering-in-a-32-bits-buffer/
- http://vemberaudio.se/graphics/RGBdiv8.pdf
