#include <homekit/homekit.h>
#include <homekit/characteristics.h>


homekit_characteristic_t cha_switch_on =
    HOMEKIT_CHARACTERISTIC_(ON, false);

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id = 1, .category = homekit_accessory_category_switch,
        .services = (homekit_service_t*[]){
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
                .characteristics = (homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "Sonoff Switch"),
                    HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Sonoff"),
                    HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "123456"),
                    HOMEKIT_CHARACTERISTIC(MODEL, "Basic R2"),
                    HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, NULL),
                    NULL
                }),
            HOMEKIT_SERVICE(SWITCH,
                .primary = true,
                .characteristics = (homekit_characteristic_t*[]){
                    &cha_switch_on,
                    NULL
                }),
            NULL
        }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};
