/**
 * @file akira_pwm_dial.c
 * @brief Generic rotary dial input driver — duty-cycle via MCPWM capture
 *
 * Reads a potentiometer-based RC oscillator whose duty cycle encodes knob
 * position.  Reports INPUT_ABS_WHEEL (0–255) into the Zephyr input subsystem,
 * exactly like the D-pad and face buttons, so any app can use the dial for
 * any purpose without knowing it is physically a brightness wheel.
 *
 * Hardware (AkiraConsole Production):
 *   RK10J12R0A0B (RV1) → D2/D3 + C67 + R58 + U5A/U5B → GPIO48 (MCPWM0 CAP0)
 *   Duty range: 9 % (CW/dim) to 91 % (CCW/bright), ~9 kHz oscillator.
 *
 * DTS node:
 *   dial0 {
 *       compatible    = "akira,pwm-dial";
 *       pwms          = <&mcpwm0 6 0 PWM_POLARITY_NORMAL>;
 *       akira,min-pct = <9>;
 *       akira,max-pct = <91>;
 *       akira,poll-ms = <50>;
 *   };
 */

#define DT_DRV_COMPAT akira_pwm_dial

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

LOG_MODULE_REGISTER(akira_pwm_dial, CONFIG_INPUT_LOG_LEVEL);

struct pwm_dial_config {
    struct pwm_dt_spec pwm;
    uint8_t  min_pct;    /* duty % at axis-0 end stop   */
    uint8_t  max_pct;    /* duty % at axis-255 end stop  */
    uint32_t poll_ms;
};

struct pwm_dial_data {
    const struct device *dev;
    struct k_work_delayable poll_work;
    int16_t last_value;   /* -1 = uninitialised */
};

/* Map duty cycle percentage to 0–255 axis value. */
static uint8_t duty_pct_to_axis(uint8_t duty_pct,
                                  uint8_t min_pct, uint8_t max_pct)
{
    if (duty_pct <= min_pct) {
        return 0;
    }
    if (duty_pct >= max_pct) {
        return 255;
    }
    return (uint8_t)(((uint32_t)(duty_pct - min_pct) * 255U)
                     / (max_pct - min_pct));
}

static void pwm_dial_poll(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct pwm_dial_data   *data   = CONTAINER_OF(dwork,
                                        struct pwm_dial_data, poll_work);
    const struct device          *dev  = data->dev;
    const struct pwm_dial_config *cfg  = dev->config;

    /* Capture one period: returns period and pulse in cycles */
    uint32_t period_cyc = 0, pulse_cyc = 0;
    int ret = pwm_capture_cycles(cfg->pwm.dev, cfg->pwm.channel,
                                  PWM_CAPTURE_TYPE_BOTH,
                                  &period_cyc, &pulse_cyc,
                                  K_MSEC(200));

    if (ret < 0) {
        /* Oscillator stalled or capture timed out — report axis 0 */
        LOG_DBG("capture failed: %d", ret);
        if (data->last_value != 0) {
            data->last_value = 0;
            input_report_abs(dev, INPUT_ABS_WHEEL, 0, true, K_FOREVER);
        }
        goto reschedule;
    }

    if (period_cyc == 0) {
        goto reschedule;
    }

    /* Duty cycle in percent (×100 for integer arithmetic, then /100) */
    uint32_t duty_pct100 = (pulse_cyc * 100U) / period_cyc;
    uint8_t  duty_pct    = (uint8_t)CLAMP(duty_pct100, 0U, 100U);
    uint8_t  axis_val    = duty_pct_to_axis(duty_pct, cfg->min_pct, cfg->max_pct);

    if (axis_val != (uint8_t)data->last_value) {
        data->last_value = axis_val;
        input_report_abs(dev, INPUT_ABS_WHEEL, axis_val, true, K_FOREVER);
        LOG_DBG("dial %s: duty=%u%% → axis=%u", dev->name, duty_pct, axis_val);
    }

reschedule:
    k_work_reschedule(&data->poll_work, K_MSEC(cfg->poll_ms));
}

static int pwm_dial_init(const struct device *dev)
{
    const struct pwm_dial_config *cfg  = dev->config;
    struct pwm_dial_data         *data = dev->data;

    if (!pwm_is_ready_dt(&cfg->pwm)) {
        LOG_ERR("PWM capture device %s not ready", cfg->pwm.dev->name);
        return -ENODEV;
    }

    data->dev        = dev;
    data->last_value = -1;

    k_work_init_delayable(&data->poll_work, pwm_dial_poll);
    k_work_schedule(&data->poll_work, K_MSEC(cfg->poll_ms));

    LOG_INF("akira_pwm_dial: %s ready (duty %u%%–%u%% → axis 0–255, %u ms poll)",
            dev->name, cfg->min_pct, cfg->max_pct, cfg->poll_ms);
    return 0;
}

#define PWM_DIAL_INIT(n)                                                    \
    static const struct pwm_dial_config pwm_dial_cfg_##n = {               \
        .pwm      = PWM_DT_SPEC_INST_GET(n),                               \
        .min_pct  = DT_INST_PROP_OR(n, akira_min_pct, 9),                  \
        .max_pct  = DT_INST_PROP_OR(n, akira_max_pct, 91),                 \
        .poll_ms  = DT_INST_PROP_OR(n, akira_poll_ms, 50),                 \
    };                                                                      \
    static struct pwm_dial_data pwm_dial_data_##n;                         \
    DEVICE_DT_INST_DEFINE(n, pwm_dial_init, NULL,                          \
                          &pwm_dial_data_##n, &pwm_dial_cfg_##n,           \
                          POST_KERNEL, 90, NULL);

DT_INST_FOREACH_STATUS_OKAY(PWM_DIAL_INIT)
