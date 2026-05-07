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

struct pwm_dial_config
{
    struct pwm_dt_spec pwm;
    uint8_t min_pct; /* duty % at axis-0 end stop   */
    uint8_t max_pct; /* duty % at axis-255 end stop  */
    uint32_t poll_ms;
};

struct pwm_dial_data
{
    const struct device *dev;
    struct k_work_delayable poll_work;
    /* Compact layout to minimise BSS contribution */
    atomic_t cap_ready;           /* 1 = callback delivered fresh data    */
    volatile uint16_t cap_period; /* period cycles (10 MHz/9 kHz ≈ 1111) */
    volatile uint16_t cap_pulse;  /* pulse cycles                         */
    int16_t last_value;           /* last reported axis; -1 = uninit      */
    uint8_t miss_count;           /* consecutive polls without signal     */
    bool cap_armed;               /* capture currently enabled            */
};

/* Map duty cycle percentage to 0–255 axis value. */
static uint8_t duty_pct_to_axis(uint8_t duty_pct,
                                uint8_t min_pct, uint8_t max_pct)
{
    if (duty_pct <= min_pct)
    {
        return 0;
    }
    if (duty_pct >= max_pct)
    {
        return 255;
    }
    return (uint8_t)(((uint32_t)(duty_pct - min_pct) * 255U) / (max_pct - min_pct));
}

/* ISR/callback: store captured values and signal the poll work. */
static void pwm_dial_capture_cb(const struct device *pwm_dev, uint32_t channel,
                                uint32_t period_cyc, uint32_t pulse_cyc,
                                int status, void *user_data)
{
    struct pwm_dial_data *data = user_data;

    ARG_UNUSED(pwm_dev);
    ARG_UNUSED(channel);

    if (status == 0 && period_cyc > 0)
    {
        data->cap_period = (uint16_t)MIN(period_cyc, (uint32_t)UINT16_MAX);
        data->cap_pulse = (uint16_t)MIN(pulse_cyc, (uint32_t)UINT16_MAX);
        atomic_set(&data->cap_ready, 1);
    }
}

/* (Re-)arm a single-shot capture.  Caller must ensure capture is disabled. */
static int arm_capture(const struct pwm_dial_config *cfg, struct pwm_dial_data *data)
{
    int ret = pwm_configure_capture(cfg->pwm.dev, cfg->pwm.channel,
                                    PWM_CAPTURE_TYPE_BOTH | PWM_CAPTURE_MODE_SINGLE,
                                    pwm_dial_capture_cb, data);
    if (ret < 0)
    {
        return ret;
    }
    return pwm_enable_capture(cfg->pwm.dev, cfg->pwm.channel);
}

static void pwm_dial_poll(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct pwm_dial_data *data = CONTAINER_OF(dwork,
                                              struct pwm_dial_data, poll_work);
    const struct device *dev = data->dev;
    const struct pwm_dial_config *cfg = dev->config;

    if (data->cap_armed)
    {
        if (atomic_cas(&data->cap_ready, 1, 0))
        {
            /* Fresh capture — signal is present */
            if (data->miss_count > 0)
            {
                LOG_INF("dial %s: PWM signal recovered", dev->name);
                data->miss_count = 0;
            }

            uint32_t period_cyc = data->cap_period;
            uint32_t pulse_cyc = data->cap_pulse;
            uint32_t duty_pct100 = (pulse_cyc * 100U) / period_cyc;
            uint8_t duty_pct = (uint8_t)CLAMP(duty_pct100, 0U, 100U);
            uint8_t axis_val = duty_pct_to_axis(duty_pct, cfg->min_pct, cfg->max_pct);

            if (axis_val != (uint8_t)data->last_value)
            {
                data->last_value = axis_val;
                input_report_abs(dev, INPUT_ABS_WHEEL, axis_val, true, K_FOREVER);
                LOG_DBG("dial %s: duty=%u%% → axis=%u", dev->name, duty_pct, axis_val);
            }
        }
        else
        {
            /* No signal within poll_ms — report axis 0 */
            if (data->miss_count < UINT8_MAX)
            {
                data->miss_count++;
            }
            if (data->miss_count == 1)
            {
                LOG_WRN("dial %s: no PWM signal", dev->name);
            }
            else
            {
                LOG_DBG("dial %s: no PWM signal (miss %u)", dev->name, data->miss_count);
            }
            if (data->last_value != 0)
            {
                data->last_value = 0;
                input_report_abs(dev, INPUT_ABS_WHEEL, 0, true, K_FOREVER);
            }
        }
        data->cap_armed = false;
    }

    /* Always disable before re-arming to ensure clean state. */
    (void)pwm_disable_capture(cfg->pwm.dev, cfg->pwm.channel);

    int ret = arm_capture(cfg, data);
    if (ret == 0)
    {
        data->cap_armed = true;
    }
    else
    {
        LOG_ERR("dial %s: capture arm failed: %d", dev->name, ret);
    }

    k_work_reschedule(&data->poll_work, K_MSEC(cfg->poll_ms));
}

static int pwm_dial_init(const struct device *dev)
{
    const struct pwm_dial_config *cfg = dev->config;
    struct pwm_dial_data *data = dev->data;

    if (!pwm_is_ready_dt(&cfg->pwm))
    {
        LOG_ERR("PWM capture device %s not ready", cfg->pwm.dev->name);
        return -ENODEV;
    }

    data->dev = dev;
    data->last_value = -1;
    atomic_set(&data->cap_ready, 0);
    data->cap_armed = false;
    data->miss_count = 0;

    k_work_init_delayable(&data->poll_work, pwm_dial_poll);
    k_work_schedule(&data->poll_work, K_MSEC(cfg->poll_ms));

    LOG_INF("akira_pwm_dial: %s ready (duty %u%%–%u%% → axis 0–255, %u ms poll)",
            dev->name, cfg->min_pct, cfg->max_pct, cfg->poll_ms);
    return 0;
}

#define PWM_DIAL_INIT(n)                                         \
    static const struct pwm_dial_config pwm_dial_cfg_##n = {     \
        .pwm = PWM_DT_SPEC_INST_GET(n),                          \
        .min_pct = DT_INST_PROP_OR(n, akira_min_pct, 9),         \
        .max_pct = DT_INST_PROP_OR(n, akira_max_pct, 91),        \
        .poll_ms = DT_INST_PROP_OR(n, akira_poll_ms, 50),        \
    };                                                           \
    static struct pwm_dial_data pwm_dial_data_##n;               \
    DEVICE_DT_INST_DEFINE(n, pwm_dial_init, NULL,                \
                          &pwm_dial_data_##n, &pwm_dial_cfg_##n, \
                          POST_KERNEL, 90, NULL);

DT_INST_FOREACH_STATUS_OKAY(PWM_DIAL_INIT)
