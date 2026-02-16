/**
 * @file shell_rf.c
 * @brief RF Module Shell Commands
 */

#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <api/akira_rf_api.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(shell_rf, LOG_LEVEL_INF);

/* Shell command: rf init <chip> */
static int cmd_rf_init(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: rf init <chip>");
        shell_print(sh, "  chip: 0=None, 1=NRF24L01, 2=CC1101, 3=LR1121");
        return -EINVAL;
    }

    int chip = atoi(argv[1]);
    if (chip < 0 || chip > 3) {
        shell_error(sh, "Invalid chip type (0-3)");
        return -EINVAL;
    }

    shell_print(sh, "Initializing RF chip %d...", chip);
    int ret = akira_rf_init((akira_rf_chip_t)chip);
    if (ret < 0) {
        shell_error(sh, "RF init failed: %d", ret);
        return ret;
    }

    shell_print(sh, "RF chip initialized successfully");
    return 0;
}

/* Shell command: rf freq <freq_hz> */
static int cmd_rf_freq(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: rf freq <freq_hz>");
        shell_print(sh, "  Example: rf freq 868000000 (868 MHz)");
        return -EINVAL;
    }

    uint32_t freq = (uint32_t)atol(argv[1]);
    shell_print(sh, "Setting frequency to %u Hz...", freq);

    int ret = akira_rf_set_frequency(freq);
    if (ret < 0) {
        shell_error(sh, "Failed to set frequency: %d", ret);
        return ret;
    }

    shell_print(sh, "Frequency set successfully");
    return 0;
}

/* Shell command: rf power <dbm> */
static int cmd_rf_power(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: rf power <dbm>");
        shell_print(sh, "  Example: rf power 14");
        return -EINVAL;
    }

    int8_t power = (int8_t)atoi(argv[1]);
    shell_print(sh, "Setting TX power to %d dBm...", power);

    int ret = akira_rf_set_power(power);
    if (ret < 0) {
        shell_error(sh, "Failed to set power: %d", ret);
        return ret;
    }

    shell_print(sh, "TX power set successfully");
    return 0;
}

/* Shell command: rf send <data> */
static int cmd_rf_send(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: rf send <data>");
        shell_print(sh, "  Example: rf send \"Hello World\"");
        return -EINVAL;
    }

    const char *data = argv[1];
    size_t len = strlen(data);

    shell_print(sh, "Sending %zu bytes...", len);

    int ret = akira_rf_send((const uint8_t *)data, len);
    if (ret < 0) {
        shell_error(sh, "Send failed: %d", ret);
        return ret;
    }

    shell_print(sh, "Data sent successfully");
    return 0;
}

/* Shell command: rf recv <timeout_ms> */
static int cmd_rf_recv(const struct shell *sh, size_t argc, char **argv)
{
    uint32_t timeout = 5000;  /* Default 5 seconds */
    if (argc >= 2) {
        timeout = (uint32_t)atol(argv[1]);
    }

    uint8_t buffer[256];
    shell_print(sh, "Receiving (timeout=%u ms)...", timeout);

    int ret = akira_rf_receive(buffer, sizeof(buffer), timeout);
    if (ret < 0) {
        shell_error(sh, "Receive failed: %d", ret);
        return ret;
    }

    if (ret == 0) {
        shell_print(sh, "No data received (timeout)");
        return 0;
    }

    shell_print(sh, "Received %d bytes:", ret);
    shell_hexdump(sh, buffer, ret);
    return 0;
}

/* Shell command: rf rssi */
static int cmd_rf_rssi(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    int16_t rssi;
    int ret = akira_rf_get_rssi(&rssi);
    if (ret < 0) {
        shell_error(sh, "Failed to read RSSI: %d", ret);
        return ret;
    }

    shell_print(sh, "RSSI: %d dBm", rssi);
    return 0;
}

/* Shell command: rf status */
static int cmd_rf_status(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(sh, "RF Status:");
    shell_print(sh, "  Framework: " 
#ifdef CONFIG_AKIRA_RF_FRAMEWORK
               "Enabled"
#else
               "Disabled"
#endif
    );
    shell_print(sh, "  LR1121: "
#ifdef CONFIG_AKIRA_LR1121
               "Available"
#else
               "Not available"
#endif
    );
    shell_print(sh, "  CC1101: "
#ifdef CONFIG_AKIRA_CC1101
               "Available"
#else
               "Not available"
#endif
    );

    return 0;
}

/* Shell command: rf deinit */
static int cmd_rf_deinit(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(sh, "Deinitializing RF...");
    int ret = akira_rf_deinit();
    if (ret < 0) {
        shell_error(sh, "RF deinit failed: %d", ret);
        return ret;
    }

    shell_print(sh, "RF deinitialized");
    return 0;
}

/* Register RF shell commands */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_rf,
    SHELL_CMD_ARG(init, NULL, "Initialize RF chip", cmd_rf_init, 2, 0),
    SHELL_CMD_ARG(deinit, NULL, "Deinitialize RF", cmd_rf_deinit, 1, 0),
    SHELL_CMD_ARG(freq, NULL, "Set frequency (Hz)", cmd_rf_freq, 2, 0),
    SHELL_CMD_ARG(power, NULL, "Set TX power (dBm)", cmd_rf_power, 2, 0),
    SHELL_CMD_ARG(send, NULL, "Send data", cmd_rf_send, 2, 0),
    SHELL_CMD_ARG(recv, NULL, "Receive data", cmd_rf_recv, 1, 1),
    SHELL_CMD_ARG(rssi, NULL, "Read RSSI", cmd_rf_rssi, 1, 0),
    SHELL_CMD_ARG(status, NULL, "Show RF status", cmd_rf_status, 1, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(rf, &sub_rf, "RF transceiver commands", NULL);
