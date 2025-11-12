#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include <zephyr/net/socket.h>

#include "wifi.h"
#include "ei_config.h"

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

/* --------------------------------------------------------------------------
 * Sampling / upload configuration
 * -------------------------------------------------------------------------- */

/* DEBUG SETTINGS: fast to verify uploads.
 * Later you can set:
 *   #define SAMPLES_PER_HOUR   10
 *   #define SAMPLE_INTERVAL_MS (360U * 1000U)   // 6 minutes
 */
#define SAMPLES_PER_HOUR          3
#define SAMPLE_INTERVAL_MS        (5U * 1000U)

/* Edge Impulse ingestion endpoint (HTTP) */
#define EI_INGEST_HOST            "ingestion.edgeimpulse.com"
#define EI_INGEST_PORT            "80"
#define EI_INGEST_PATH            "/api/training/data"
#define EI_DEVICE_NAME            "esp32s3-zephyr"
#define EI_DEVICE_TYPE            "ESP32S3"

struct sample_entry {
    uint32_t t_ms;
    double   temp_c;
    double   hum_pct;
};

static struct sample_entry samples[SAMPLES_PER_HOUR];
static int sample_count = 0;

/* --------------------------------------------------------------------------
 * Label + JSON builder
 * -------------------------------------------------------------------------- */

static void make_label(char *buf, size_t len)
{
    time_t now = time(NULL);
    if (now == (time_t)-1) {
        uint32_t up_s = k_uptime_get_32() / 1000U;
        snprintk(buf, len, "session_%u", up_s);
        return;
    }

    struct tm tm_buf;
    struct tm *tm = gmtime_r(&now, &tm_buf);
    if (!tm) {
        uint32_t up_s = k_uptime_get_32() / 1000U;
        snprintk(buf, len, "session_%u", up_s);
        return;
    }

    static const char *wdays[] = {
        "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
    };
    const char *wday = "Day";
    if (tm->tm_wday >= 0 && tm->tm_wday < 7) {
        wday = wdays[tm->tm_wday];
    }

    snprintk(buf, len, "%s_%04d-%02d-%02d_%02d-%02d-%02d",
             wday,
             tm->tm_year + 1900,
             tm->tm_mon + 1,
             tm->tm_mday,
             tm->tm_hour,
             tm->tm_min,
             tm->tm_sec);
}

static void flash_led_quick(void)
{
    gpio_pin_set_dt(&led, 1);
    k_sleep(K_MSEC(200));
    gpio_pin_set_dt(&led, 0);
}

static int build_ei_json(char *out, size_t out_size,
                         const struct sample_entry *buf,
                         int count)
{
    int len = 0;
    int rem = (int)out_size;

    len = snprintk(out, rem,
                   "{"
                   "\"protected\":{"
                     "\"ver\":\"v1\","
                     "\"alg\":\"none\","
                     "\"iat\":0"
                   "},"
                   "\"signature\":\"0\","
                   "\"payload\":{"
                     "\"device_name\":\"%s\","
                     "\"device_type\":\"%s\","
                     "\"interval_ms\":%u,"
                     "\"sensors\":["
                       "{\"name\":\"temp\",\"units\":\"C\"},"
                       "{\"name\":\"hum\",\"units\":\"%%\"}"
                     "],"
                     "\"values\":[",
                   EI_DEVICE_NAME,
                   EI_DEVICE_TYPE,
                   SAMPLE_INTERVAL_MS);
    if (len < 0 || len >= rem) {
        return -1;
    }
    rem -= len;

    for (int i = 0; i < count; i++) {
        int n = snprintk(out + len, rem,
                         "[%.5f,%.5f]%s",
                         buf[i].temp_c,
                         buf[i].hum_pct,
                         (i == count - 1) ? "" : ",");
        if (n < 0 || n >= rem) {
            return -1;
        }
        len += n;
        rem -= n;
    }

    int n = snprintk(out + len, rem, "]}}");
    if (n < 0 || n >= rem) {
        return -1;
    }
    len += n;
    return len;
}

static int upload_to_edge_impulse(const struct sample_entry *buf,
                                  int count,
                                  const char *label)
{
    printk("Uploading %d samples to Edge Impulse with label '%s'\n",
           count, label);

    char body[2048];
    int body_len = build_ei_json(body, sizeof(body), buf, count);
    if (body_len < 0) {
        printk("Failed to build JSON body\n");
        return -1;
    }

    struct zsock_addrinfo hints;
    struct zsock_addrinfo *res = NULL;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int err = zsock_getaddrinfo(EI_INGEST_HOST, EI_INGEST_PORT,
                                &hints, &res);
    if (err != 0 || res == NULL) {
        printk("zsock_getaddrinfo failed: %d\n", err);
        if (res) {
            zsock_freeaddrinfo(res);
        }
        return -1;
    }

    int sock = zsock_socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        printk("zsock_socket() failed: %d\n", errno);
        zsock_freeaddrinfo(res);
        return -1;
    }

    if (zsock_connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        printk("zsock_connect() failed: %d\n", errno);
        zsock_close(sock);
        zsock_freeaddrinfo(res);
        return -1;
    }

    zsock_freeaddrinfo(res);

    char req[4096];
    int req_len = snprintk(req, sizeof(req),
                           "POST " EI_INGEST_PATH " HTTP/1.1\r\n"
                           "Host: " EI_INGEST_HOST "\r\n"
                           "Connection: close\r\n"
                           "x-api-key: " EI_API_KEY "\r\n"
                           "x-label: %s\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: %d\r\n"
                           "\r\n",
                           label, body_len);
    if (req_len < 0 || req_len >= (int)sizeof(req)) {
        printk("Failed to build HTTP headers\n");
        zsock_close(sock);
        return -1;
    }

    int sent = zsock_send(sock, req, req_len, 0);
    if (sent != req_len) {
        printk("Failed to send HTTP headers (sent=%d, expected=%d)\n",
               sent, req_len);
        zsock_close(sock);
        return -1;
    }

    sent = zsock_send(sock, body, body_len, 0);
    if (sent != body_len) {
        printk("Failed to send HTTP body (sent=%d, expected=%d)\n",
               sent, body_len);
        zsock_close(sock);
        return -1;
    }

    char resp[256];
    int r = zsock_recv(sock, resp, sizeof(resp) - 1, 0);
    if (r > 0) {
        resp[r] = '\0';
        printk("EI response: %s\n", resp);
    } else {
        printk("EI recv returned %d\n", r);
    }

    zsock_close(sock);
    return 0;
}

/* --------------------------------------------------------------------------
 * Main
 * -------------------------------------------------------------------------- */

int main(void)
{
    int ret;

    printk("Edge Impulse ESP32S3 temp/humidity logger starting\n");

    if (!device_is_ready(led.port) ||
        !device_is_ready(button.port) ||
        !device_is_ready(ths_dev)) {
        printk("Devices not ready\n");
        return 0;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        printk("Failed to configure LED: %d\n", ret);
        return 0;
    }

    ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (ret != 0) {
        printk("Failed to configure button: %d\n", ret);
        return 0;
    }

    /* Startup indication: flash LED twice */
    gpio_pin_set_dt(&led, 1);
    k_sleep(K_SECONDS(1));
    gpio_pin_set_dt(&led, 0);
    k_sleep(K_SECONDS(1));
    gpio_pin_set_dt(&led, 1);
    k_sleep(K_SECONDS(1));
    gpio_pin_set_dt(&led, 0);

    /* Idle state: LED ON when *not* sampling */
    gpio_pin_set_dt(&led, 1);

    /* Bring up Wi-Fi using your known-good helpers, but don’t block the app forever */
    wifi_init();
    printk("Connecting to WiFi SSID='%s'...\n", WIFI_SSID);
    ret = wifi_connect(WIFI_SSID, WIFI_PASS);
    if (ret < 0) {
        printk("WiFi connection failed (%d), uploads will fail but sampling will still run\n", ret);
    } else {
        printk("WiFi connect() returned %d, waiting for IP...\n", ret);
        wifi_wait_for_ip_addr();
        printk("WiFi ready, continuing.\n");
    }

    bool last_pressed = false;
    bool sampling_enabled = false;
    uint32_t last_sample_ms = 0;

    while (1) {
        int val = gpio_pin_get_dt(&button);
        if (val >= 0) {
            bool pressed = (val == 0); /* active low */

            if (pressed && !last_pressed) {
                printk("Button press edge detected (sampling=%d)\n",
                       sampling_enabled ? 1 : 0);

                if (!sampling_enabled) {
                    sampling_enabled = true;
                    sample_count = 0;
                    last_sample_ms = k_uptime_get_32();
                    gpio_pin_set_dt(&led, 0);  /* LED off while sampling */
                    printk("Sampling started\n");
                } else {
                    sampling_enabled = false;
                    gpio_pin_set_dt(&led, 1);  /* LED on when stopped */
                    printk("Sampling stopped\n");
                }
            }

            last_pressed = pressed;
        }

        if (sampling_enabled) {
            uint32_t now_ms = k_uptime_get_32();

            if (sample_count == 0 ||
                (now_ms - last_sample_ms) >= SAMPLE_INTERVAL_MS) {

                struct sensor_value temp, hum;

                ret = sensor_sample_fetch(ths_dev);
                if (ret == 0) {
                    ret = sensor_channel_get(ths_dev,
                                             SENSOR_CHAN_AMBIENT_TEMP,
                                             &temp);
                }
                if (ret == 0) {
                    ret = sensor_channel_get(ths_dev,
                                             SENSOR_CHAN_HUMIDITY,
                                             &hum);
                }

                if (ret == 0 && sample_count < SAMPLES_PER_HOUR) {
                    double temp_c = sensor_value_to_double(&temp);
                    double hum_pct = sensor_value_to_double(&hum);

                    samples[sample_count].t_ms    = now_ms;
                    samples[sample_count].temp_c  = temp_c;
                    samples[sample_count].hum_pct = hum_pct;
                    sample_count++;

                    last_sample_ms = now_ms;

                    printk("Sample %d: T=%.2f C, RH=%.2f %%\n",
                           sample_count, temp_c, hum_pct);
                }

                if (sample_count >= SAMPLES_PER_HOUR) {
                    char label[64];
                    make_label(label, sizeof(label));

                    int up_ret = upload_to_edge_impulse(samples,
                                                        sample_count,
                                                        label);

                    printk("Upload done (ret=%d), label='%s'\n",
                           up_ret, label);

                    flash_led_quick();

                    sample_count = 0;
                    last_sample_ms = k_uptime_get_32();
                }
            }
        }

        k_msleep(100);
    }

    return 0;
}
