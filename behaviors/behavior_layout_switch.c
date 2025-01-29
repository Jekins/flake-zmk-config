#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/endpoints.h>
#include <zmk/layers.h>

/*
 * Поведение делает следующее:
 *
 * 1) TAP (короткое нажатие < tapping_term):
 *    - При отпускании => переходим в "tap_layer" (его номер передаём через параметр в keymap).
 *
 * 2) HOLD (удержание дольше tapping_term):
 *    - Спустя tapping_term после нажатия => отправляем LGUI(SPACE), включаем слой #2.
 *    - При отпускании (уже после HOLD) => ещё раз LGUI(SPACE) и выключаем слой #2.
 *
 * То есть "hold"‑слой = 2 (зашит в коде), а "tap"‑слой мы передаём через binding->parameters[0].
 */

#define LAYOUT_SWITCH_TAPPING_TERM 200

static struct k_work_delayable hold_timer_work;    /* отложенная задача для определения hold */
static bool button_held = false;                   /* кнопка всё ещё удерживается? */
static bool hold_active = false;                   /* уже перешли в hold-сценарий? */
static uint32_t pressed_time = 0;                  /* время нажатия */

static struct zmk_behavior_binding current_binding;
static struct zmk_behavior_binding_event current_event;

/* Жёстко зашитый слой для hold */
static const uint8_t HOLD_LAYER = 2;

/* Функция, «нажимающая» и «отпускающая» LGUI(SPACE). */
static void send_lgui_space_press_release(void) {
    zmk_keymap_press(KC_LGUI);
    zmk_keymap_press(KC_SPACE);
    zmk_keymap_release(KC_SPACE);
    zmk_keymap_release(KC_LGUI);
}

/* Эта функция срабатывает по истечении tapping_term */
static void hold_timer_func(struct k_work *work) {
    /* Если кнопку всё ещё не отпустили, значит это HOLD. */
    if (button_held) {
        hold_active = true;

        /* При HOLD сразу жмём LGUI(SPC) и включаем слой 2. */
        send_lgui_space_press_release();
        zmk_layer_on(HOLD_LAYER);
    }
}

static int layout_switch_key_pressed(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event)
{
    /* Запомним текущую binding/event, чтобы в hold_timer_func можно было к ним обратиться. */
    current_binding = *binding;
    current_event = event;

    pressed_time = k_uptime_get_32();
    button_held = true;
    hold_active = false;

    /* Запускаем отложенную задачу: если через LAYOUT_SWITCH_TAPPING_TERM кнопку не отпустили,
     * значит это HOLD, и мы выполним соответствующую логику.
     */
    k_work_schedule(&hold_timer_work, K_MSEC(LAYOUT_SWITCH_TAPPING_TERM));

    return ZMK_BEHAVIOR_OPAQUE;
}

static int layout_switch_key_released(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event)
{
    /* Узнаём, сколько прошло времени */
    uint32_t now = k_uptime_get_32();
    uint32_t delta = now - pressed_time;

    /* Фиксируем, что кнопку отпустили (для hold_timer_func) */
    button_held = false;
    /* Если hold_timer_func ещё не успел сработать, отменяем отложенную задачу */
    k_work_cancel_delayable(&hold_timer_work);

    /* Слой для ТАПА возьмём из параметра #0 (binding->parameters[0]) */
    uint8_t tap_layer = binding->parameters[0];

    if (!hold_active && delta < LAYOUT_SWITCH_TAPPING_TERM) {
        /* ===== TAP-СЦЕНАРИЙ ===== */
        /* Короткое нажатие => переходим на tap_layer */
        zmk_layer_clear();
        zmk_layer_move(tap_layer);

    } else if (hold_active) {
        /* ===== HOLD-СЦЕНАРИЙ (отпускание) ===== */
        /* Мы уже отправили LGUI(SPC) и включили HOLD_LAYER.
         * Теперь при отпускании - снова LGUI(SPC) и выключаем HOLD_LAYER
         */
        send_lgui_space_press_release();
        zmk_layer_off(HOLD_LAYER);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

/* Регистрация драйвера поведения */
static const struct behavior_driver_api layout_switch_driver_api = {
    .binding_pressed = layout_switch_key_pressed,
    .binding_released = layout_switch_key_released,
};

/* Инициализация, создаём отложенную задачу. */
static int layout_switch_init(const struct device *dev) {
    k_work_init_delayable(&hold_timer_work, hold_timer_func);
    return 0;
}

DEVICE_DT_INST_DEFINE(0,
    layout_switch_init,
    NULL,
    NULL,
    NULL,
    POST_KERNEL,
    CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
    &layout_switch_driver_api
);