# Bộ nhỏ
python3 png_to_rgb565.py --size 22 \
    --symbol-prefix ICONSM_ --size-name WX_ICON_SM_SIZE \
    --out weather_icons_sm.h \
    icon_sunny.png icon_sunny_cloud.png icon_cloudy.png \
    icon_rainny.png icon_haze.png icon_windy.png

# Bộ lớn
python3 png_to_rgb565.py --size 40 \
    --out weather_icons.h \
    icon_sunny.png icon_sunny_cloud.png icon_cloudy.png \
    icon_rainny.png icon_haze.png icon_windy.png