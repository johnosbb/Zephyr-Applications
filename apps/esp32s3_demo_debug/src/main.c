#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>

/* Devicetree aliases from overlay */
#define LED0_NODE DT_ALIAS(led0)
#define SW0_NODE  DT_ALIAS(sw0)
#define THS0_NODE DT_ALIAS(ths0)

#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "No alias 'led0' in devicetree; check overlay."
#endif

#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "No alias 'sw0' in devicetree; check overlay."
#endif

#if !DT_NODE_HAS_STATUS(THS0_NODE, okay)
#error "No alias 'ths0' in devicetree; check overlay."
#endif

static const struct gpio_dt_spec led =
    GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static const struct gpio_dt_spec button =
    GPIO_DT_SPEC_GET(SW0_NODE, gpios);

static const struct device *const ths_dev =
    DEVICE_DT_GET(THS0_NODE);



int main(void)
{
    int ret;

    printk("ESP32S3 demo: LED, button, SHT40 sensor\n");

    if (!device_is_ready(led.port)) {
        printk("LED device not ready\n");
        return 0;
    }

    if (!device_is_ready(button.port)) {
        printk("Button device not ready\n");
        return 0;
    }

    if (!device_is_ready(ths_dev)) {
        printk("SHT40 device not ready\n");
        return 0;
    }

    /* LED output, initially off */
    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        printk("Failed to configure LED: %d\n", ret);
        return 0;
    }

    /* Button input, pulls from devicetree */
    ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (ret != 0) {
        printk("Failed to configure button: %d\n", ret);
        return 0;
    }

    bool last_pressed = false;
    int counter = 0;

    while (1) {
        /* Button handling */
        int val = gpio_pin_get_dt(&button);
        if (val >= 0) {
            bool pressed = (val == 0); /* because active low */
            if (pressed != last_pressed) {
                last_pressed = pressed;
                printk("Button is %s\n", pressed ? "PRESSED" : "released");
                gpio_pin_set_dt(&led, pressed ? 1 : 0);
            }
        }

        /* Every 1 second, read SHT40 */
        if ((counter % 10) == 0) {
            struct sensor_value temp, hum;

            ret = sensor_sample_fetch(ths_dev);
            if (ret == 0) {
                ret = sensor_channel_get(ths_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
            }
            if (ret == 0) {
                ret = sensor_channel_get(ths_dev, SENSOR_CHAN_HUMIDITY, &hum);
            }

            if (ret == 0) {
                /* temp.val2 and hum.val2 are in micro-units (1e-6) */
                int t_int = temp.val1;
                int t_dec = temp.val2 / 10000;   /* two decimals: micro / 10^4 */

                int h_int = hum.val1;
                int h_dec = hum.val2 / 10000;

                printk("SHT40: T = %d.%02d C, RH = %d.%02d %%\n",
                    t_int, t_dec, h_int, h_dec);
            } else {
                printk("SHT40 read error: %d\n", ret);
            }
        }

        counter++;
        k_msleep(100); /* 100 ms loop => ~1 s per SHT40 read */
    }

    return 0;
}
