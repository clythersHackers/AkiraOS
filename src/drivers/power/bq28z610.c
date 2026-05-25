/*
 * Copyright (c) 2026 PenEngineering S.R.L
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_bq28z610

/**
 * @file bq28z610.c
 * @brief TI BQ28Z610DRZ 1-cell Li-Ion fuel gauge + protection driver.
 *
 * The BQ28Z610 implements the SBS (Smart Battery System) 1.1 command set,
 * so all standard property reads use the standard SBS register addresses.
 *
 * I2C address: 0x55 (7-bit).
 * Sense resistor: 1 mΩ (R16 on AkiraConsole Production board).
 *
 * Read protocol: write 1-byte register address, read 2-byte little-endian value.
 * The device uses standard I2C (not SMBus PEC) when accessed over I2C.
 *
 * Supported properties (read-only):
 *   FUEL_GAUGE_VOLTAGE                   → battery voltage (µV)
 *   FUEL_GAUGE_CURRENT                   → instantaneous current (µA, signed)
 *   FUEL_GAUGE_AVG_CURRENT               → 1-minute average current (µA, signed)
 *   FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE  → SoC percentage (0–100)
 *   FUEL_GAUGE_ABSOLUTE_STATE_OF_CHARGE  → absolute SoC (0–100)
 *   FUEL_GAUGE_REMAINING_CAPACITY        → remaining capacity (µAh)
 *   FUEL_GAUGE_FULL_CHARGE_CAPACITY      → full charge capacity (µAh)
 *   FUEL_GAUGE_RUNTIME_TO_EMPTY          → time to empty (minutes)
 *   FUEL_GAUGE_RUNTIME_TO_FULL           → average time to full (minutes)
 *   FUEL_GAUGE_TEMPERATURE               → cell temperature (0.1 K units)
 *   FUEL_GAUGE_CYCLE_COUNT               → charge cycle count
 *   FUEL_GAUGE_PRESENT_STATE             → always FUEL_GAUGE_BATTERY_PRESENT
 */

#include <zephyr/device.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(bq28z610, CONFIG_FUEL_GAUGE_LOG_LEVEL);

/* =========================================================================
 * SBS register map (all 16-bit, little-endian)
 * ========================================================================= */
#define BQ28Z610_REG_TEMPERATURE            0x06  /* 0.1 K */
#define BQ28Z610_REG_VOLTAGE                0x08  /* mV */
#define BQ28Z610_REG_CURRENT                0x0A  /* mA, signed */
#define BQ28Z610_REG_RELATIVE_SOC           0x0C  /* % */
#define BQ28Z610_REG_ABSOLUTE_SOC           0x0E  /* % */
#define BQ28Z610_REG_REMAINING_CAPACITY     0x10  /* mAh */
#define BQ28Z610_REG_FULL_CHARGE_CAPACITY   0x12  /* mAh */
#define BQ28Z610_REG_RUNTIME_TO_EMPTY       0x14  /* min */
#define BQ28Z610_REG_AVG_TIME_TO_EMPTY      0x16  /* min */
#define BQ28Z610_REG_AVG_TIME_TO_FULL       0x18  /* min */
#define BQ28Z610_REG_AVG_CURRENT            0x1A  /* mA, signed */
#define BQ28Z610_REG_CYCLE_COUNT            0x2A  /* count */
#define BQ28Z610_REG_DESIGN_CAPACITY        0x3C  /* mAh */
#define BQ28Z610_REG_DESIGN_VOLTAGE         0x3E  /* mV */

/* =========================================================================
 * Driver config / data
 * ========================================================================= */
struct bq28z610_config {
    struct i2c_dt_spec i2c;
};

/* ---------- helpers ---------- */

static int bq28z610_reg_read(const struct device *dev, uint8_t reg, uint16_t *val)
{
    const struct bq28z610_config *cfg = dev->config;
    uint8_t buf[2];

    int ret = i2c_burst_read_dt(&cfg->i2c, reg, buf, sizeof(buf));
    if (ret < 0) {
        LOG_ERR("I2C read reg 0x%02X failed: %d", reg, ret);
        return ret;
    }

    *val = sys_get_le16(buf);
    return 0;
}

/* ---------- fuel_gauge_get_property ---------- */

static int bq28z610_get_prop(const struct device *dev, fuel_gauge_prop_t prop,
                             union fuel_gauge_prop_val *val)
{
    uint16_t raw = 0;
    int ret;

    switch (prop) {
    case FUEL_GAUGE_VOLTAGE:
        ret = bq28z610_reg_read(dev, BQ28Z610_REG_VOLTAGE, &raw);
        if (ret == 0) {
            /* BQ28Z610 reports mV; Zephyr wants µV */
            val->voltage = (int)raw * 1000;
        }
        return ret;

    case FUEL_GAUGE_CURRENT:
        ret = bq28z610_reg_read(dev, BQ28Z610_REG_CURRENT, &raw);
        if (ret == 0) {
            /* BQ28Z610 reports mA (signed); Zephyr wants µA */
            val->current = (int)(int16_t)raw * 1000;
        }
        return ret;

    case FUEL_GAUGE_AVG_CURRENT:
        ret = bq28z610_reg_read(dev, BQ28Z610_REG_AVG_CURRENT, &raw);
        if (ret == 0) {
            val->avg_current = (int)(int16_t)raw * 1000;
        }
        return ret;

    case FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE:
        ret = bq28z610_reg_read(dev, BQ28Z610_REG_RELATIVE_SOC, &raw);
        if (ret == 0) {
            val->relative_state_of_charge = (uint8_t)CLAMP(raw, 0, 100);
        }
        return ret;

    case FUEL_GAUGE_ABSOLUTE_STATE_OF_CHARGE:
        ret = bq28z610_reg_read(dev, BQ28Z610_REG_ABSOLUTE_SOC, &raw);
        if (ret == 0) {
            val->absolute_state_of_charge = (uint8_t)CLAMP(raw, 0, 100);
        }
        return ret;

    case FUEL_GAUGE_REMAINING_CAPACITY:
        ret = bq28z610_reg_read(dev, BQ28Z610_REG_REMAINING_CAPACITY, &raw);
        if (ret == 0) {
            /* BQ28Z610 reports mAh; Zephyr wants µAh */
            val->remaining_capacity = (uint32_t)raw * 1000;
        }
        return ret;

    case FUEL_GAUGE_FULL_CHARGE_CAPACITY:
        ret = bq28z610_reg_read(dev, BQ28Z610_REG_FULL_CHARGE_CAPACITY, &raw);
        if (ret == 0) {
            val->full_charge_capacity = (uint32_t)raw * 1000;
        }
        return ret;

    case FUEL_GAUGE_RUNTIME_TO_EMPTY:
        ret = bq28z610_reg_read(dev, BQ28Z610_REG_RUNTIME_TO_EMPTY, &raw);
        if (ret == 0) {
            /* 0xFFFF = not discharging / time unknown */
            val->runtime_to_empty = (raw == 0xFFFF) ? 0 : (uint32_t)raw;
        }
        return ret;

    case FUEL_GAUGE_RUNTIME_TO_FULL:
        ret = bq28z610_reg_read(dev, BQ28Z610_REG_AVG_TIME_TO_FULL, &raw);
        if (ret == 0) {
            val->runtime_to_full = (raw == 0xFFFF) ? 0 : (uint32_t)raw;
        }
        return ret;

    case FUEL_GAUGE_TEMPERATURE:
        ret = bq28z610_reg_read(dev, BQ28Z610_REG_TEMPERATURE, &raw);
        if (ret == 0) {
            /* BQ28Z610 reports in 0.1 K; Zephyr fuel_gauge uses same unit */
            val->temperature = raw;
        }
        return ret;

    case FUEL_GAUGE_CYCLE_COUNT:
        ret = bq28z610_reg_read(dev, BQ28Z610_REG_CYCLE_COUNT, &raw);
        if (ret == 0) {
            val->cycle_count = (uint32_t)raw;
        }
        return ret;

    case FUEL_GAUGE_PRESENT_STATE:
        /* Device responded on I2C → battery is present */
        val->present_state = true;
        return 0;

    default:
        return -ENOTSUP;
    }
}

/* ---------- init ---------- */

static int bq28z610_init(const struct device *dev)
{
    const struct bq28z610_config *cfg = dev->config;

    if (!i2c_is_ready_dt(&cfg->i2c)) {
        LOG_ERR("I2C bus %s not ready", cfg->i2c.bus->name);
        return -ENODEV;
    }

    /* Verify the gauge responds by reading voltage */
    uint16_t volt = 0;
    int ret = bq28z610_reg_read(dev, BQ28Z610_REG_VOLTAGE, &volt);
    if (ret < 0) {
        LOG_WRN("BQ28Z610 not responding (I2C 0x%02X): %d — "
                "battery may be absent or gauge in shutdown",
                cfg->i2c.addr, ret);
        /* Return 0: init passes even if the gauge is absent at boot
         * (power_manager checks device_is_ready at runtime). */
        return 0;
    }

    LOG_INF("BQ28Z610 ready — Vbat=%u mV", volt);
    return 0;
}

/* ---------- driver registration ---------- */

static DEVICE_API(fuel_gauge, bq28z610_driver_api) = {
    .get_property = bq28z610_get_prop,
};

#define BQ28Z610_INIT(inst)                                                  \
    static const struct bq28z610_config bq28z610_config_##inst = {          \
        .i2c = I2C_DT_SPEC_INST_GET(inst),                                  \
    };                                                                       \
    DEVICE_DT_INST_DEFINE(inst, bq28z610_init, NULL,                        \
                          NULL, &bq28z610_config_##inst,                    \
                          POST_KERNEL, CONFIG_FUEL_GAUGE_INIT_PRIORITY,     \
                          &bq28z610_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BQ28Z610_INIT)
