// behavior_lg_space_layer.c

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/endian.h>

// Структура состояния поведения
struct behavior_lg_space_layer_state {
    bool is_hold;
};

// Инициализация поведения
static int behavior_lg_space_layer_init(const struct device *dev) {
    struct behavior_lg_space_layer_state *state = dev->data;
    state->is_hold = false;
    return 0;
}

// Функция отправки комбинации LGUI + SPACE
static void send_lg_space(const struct device *dev) {
    zmk_mod_state_update(ZMK_MOD_LGUI, true);
    zmk_keycode_send(ZMK_KEY(KC_SPACE), ZMK_MOD_LGUI);
    zmk_mod_state_update(ZMK_MOD_LGUI, false);
}

// Обработчик нажатия клавиши
static int behavior_lg_space_layer_binding_pressed(struct zmk_behavior_binding *binding,
                                                    struct zmk_behavior_binding_event event) {
    struct behavior_lg_space_layer_state *state = binding->behavior->state;

    if (!state->is_hold) {
        send_lg_space(binding->behavior->dev);
        state->is_hold = true;
        // Активируем слой 1
        zmk_layer_on(1);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

// Обработчик отпускания клавиши
static int behavior_lg_space_layer_binding_released(struct zmk_behavior_binding *binding,
                                                     struct zmk_behavior_binding_event event) {
    struct behavior_lg_space_layer_state *state = binding->behavior->state;

    if (state->is_hold) {
        send_lg_space(binding->behavior->dev);
        state->is_hold = false;
        // Деактивируем слой 1
        zmk_layer_off(1);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

// Определение API поведения
static const struct behavior_driver_api behavior_lg_space_layer_api = {
    .binding_pressed = behavior_lg_space_layer_binding_pressed,
    .binding_released = behavior_lg_space_layer_binding_released,
};

// Структура данных поведения
static struct behavior_lg_space_layer_state behavior_lg_space_layer_data;

// Регистрация устройства поведения
DEVICE_DEFINE(behavior_lg_space_layer, "zmk,behavior-lg-space-layer",
              behavior_lg_space_layer_init, NULL,
              &behavior_lg_space_layer_data, NULL,
              APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
              &behavior_lg_space_layer_api);