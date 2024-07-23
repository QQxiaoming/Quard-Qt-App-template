# the convert command comes from imagemagick
for size in 16 32 64 128 256; do
  half="$(($size / 2))"
  convert ico.png -resize x$size ico${size}.png
done

convert ico16.png ico32.png ico64.png ico128.png ico256.png ico.ico
