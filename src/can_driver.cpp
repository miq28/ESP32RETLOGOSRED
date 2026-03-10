#include "can_driver.h"
#include "driver/gpio.h"

#define CAN_RX_PIN GPIO_NUM_26
#define CAN_TX_PIN GPIO_NUM_27

static bool running = false;

static twai_timing_config_t timing(uint32_t speed)
{
    switch(speed)
    {
        case 1000000: return TWAI_TIMING_CONFIG_1MBITS();
        case 800000:  return TWAI_TIMING_CONFIG_800KBITS();
        case 500000:  return TWAI_TIMING_CONFIG_500KBITS();
        case 250000:  return TWAI_TIMING_CONFIG_250KBITS();
        case 125000:  return TWAI_TIMING_CONFIG_125KBITS();
        case 100000:  return TWAI_TIMING_CONFIG_100KBITS();
        case 50000:   return TWAI_TIMING_CONFIG_50KBITS();
        case 25000:   return TWAI_TIMING_CONFIG_25KBITS();
        default:      return TWAI_TIMING_CONFIG_500KBITS();
    }
}

bool can_init(uint32_t speed)
{
    twai_general_config_t g_config =
        TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);

    g_config.rx_queue_len = 128;
    g_config.tx_queue_len = 16;

    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    twai_timing_config_t t_config = timing(speed);

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK)
        return false;

    if (twai_start() != ESP_OK)
        return false;

    running = true;
    return true;
}

void can_stop()
{
    if (!running) return;

    twai_stop();
    twai_driver_uninstall();
    running = false;
}

bool can_set_speed(uint32_t speed)
{
    can_stop();
    return can_init(speed);
}

bool can_send(uint32_t id, bool ext, bool rtr, uint8_t len, uint8_t *data)
{
    twai_message_t msg = {0};

    msg.identifier = id;
    msg.extd = ext;
    msg.rtr = rtr;
    msg.data_length_code = len;

    for(int i = 0; i < len; i++)
        msg.data[i] = data[i];

    return twai_transmit(&msg, pdMS_TO_TICKS(4)) == ESP_OK;
}