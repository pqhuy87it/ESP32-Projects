#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *obj0;
    lv_obj_t *label_location;
    lv_obj_t *label_time;
    lv_obj_t *label_date;
    lv_obj_t *image_icon_weather;
    lv_obj_t *label_weather_main;
    lv_obj_t *label_weather_description;
    lv_obj_t *obj1;
    lv_obj_t *label_temperature;
    lv_obj_t *obj2;
    lv_obj_t *obj3;
    lv_obj_t *label_feels_like;
    lv_obj_t *obj4;
    lv_obj_t *obj5;
    lv_obj_t *label_temp_max;
    lv_obj_t *obj6;
    lv_obj_t *obj7;
    lv_obj_t *label_temp_min;
    lv_obj_t *obj8;
    lv_obj_t *label_humidity;
    lv_obj_t *obj9;
    lv_obj_t *obj10;
    lv_obj_t *label_wind;
    lv_obj_t *obj11;
    lv_obj_t *obj12;
    lv_obj_t *label_pressure;
    lv_obj_t *obj13;
    lv_obj_t *obj14;
    lv_obj_t *label_visibility;
    lv_obj_t *obj15;
    lv_obj_t *label_info;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_MAIN = 1,
};

void create_screen_main();
void tick_screen_main();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/