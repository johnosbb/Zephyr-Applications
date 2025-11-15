#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/wifi_mgmt.h>

/* Semaphore signalled when Wi-Fi connect result arrives */
static K_SEM_DEFINE(wifi_connected, 0, 1);

/* Wi-Fi management callback */
static struct net_mgmt_event_callback wifi_cb;

/* Last connection status from driver (0 = OK, otherwise error code) */
static int wifi_connect_result = -1;

/* --------------------------- Event handler helpers -------------------------- */
static void handle_wifi_connect_result(struct net_mgmt_event_callback *cb)
{
    const struct wifi_status *status = (const struct wifi_status *)cb->info;
    int s = status ? status->status : -1;

    wifi_connect_result = s;

    if (s) {
        printk("WiFi connect result: error=%d\n", s);
    } else {
        printk("WiFi connect result: success\n");
    }

    k_sem_give(&wifi_connected);
}

static void handle_wifi_disconnect_result(struct net_mgmt_event_callback *cb)
{
    const struct wifi_status *status = (const struct wifi_status *)cb->info;
    int s = status ? status->status : 0;

    if (s) {
        printk("WiFi disconnect result: error=%d\n", s);
    } else {
        printk("WiFi disconnected\n");
        k_sem_take(&wifi_connected, K_NO_WAIT);
    }
}

/* Single net-mgmt event handler, like Zephyr_WiFi */
static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                    uint64_t mgmt_event,
                                    struct net_if *iface)
{
    switch (mgmt_event) {
    case NET_EVENT_WIFI_CONNECT_RESULT:
        handle_wifi_connect_result(cb);
        break;
    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        handle_wifi_disconnect_result(cb);
        break;
    default:
        break;
    }
}

/* Initialize Wi-Fi management callbacks */
void wifi_init(void)
{
    net_mgmt_init_event_callback(&wifi_cb,
                                 wifi_mgmt_event_handler,
                                 NET_EVENT_WIFI_CONNECT_RESULT |
                                 NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_cb);
}

int wifi_connect(char *ssid, char *psk)
{
    int ret;
    struct net_if *iface;
    struct wifi_connect_req_params params;

    iface = net_if_get_default();

    memset(&params, 0, sizeof(params));
    params.ssid = (const uint8_t *)ssid;
    params.ssid_length = strlen(ssid);
    params.psk = (const uint8_t *)psk;
    params.psk_length = strlen(psk);
    params.security = WIFI_SECURITY_TYPE_PSK;
    params.band = WIFI_FREQ_BAND_2_4_GHZ;
    params.channel = WIFI_CHANNEL_ANY;
    params.mfp = WIFI_MFP_OPTIONAL;

    wifi_connect_result = -1;

    ret = net_mgmt(NET_REQUEST_WIFI_CONNECT,
                   iface,
                   &params,
                   sizeof(params));
    if (ret) {
        printk("net_mgmt WIFI_CONNECT failed immediately: %d\n", ret);
        return ret;
    }

    /* Wait up to 15 seconds for connect result event */
    if (k_sem_take(&wifi_connected, K_SECONDS(15)) != 0) {
        printk("Timeout waiting for WiFi connect result\n");
        return -ETIMEDOUT;
    }

    if (wifi_connect_result != 0) {
        printk("WiFi connect failed with status=%d\n", wifi_connect_result);
        return -wifi_connect_result;
    }

    return 0;
}

/* Static IPv4 info print, similar to Zephyr_WiFi */
void wifi_wait_for_ip_addr(void)
{
    struct net_if *iface = net_if_get_default();
    struct net_if_ipv4 *ipv4 = iface->config.ip.ipv4;

    char buf[NET_IPV4_ADDR_LEN];

    for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
        if (ipv4->unicast[i].ipv4.addr_type != NET_ADDR_MANUAL) {
            continue;
        }
        if (net_ipv4_is_addr_unspecified(&ipv4->unicast[i].ipv4.address.in_addr)) {
            continue;
        }

        printk("IPv4 address (Static): %s\n",
               net_addr_ntop(AF_INET,
                             &ipv4->unicast[i].ipv4.address.in_addr,
                             buf, sizeof(buf)));

        printk("Subnet: %s\n",
               net_addr_ntop(AF_INET,
                             &ipv4->unicast[i].netmask,
                             buf, sizeof(buf)));

        printk("Router: %s\n",
               net_addr_ntop(AF_INET,
                             &ipv4->gw,
                             buf, sizeof(buf)));
    }
}

// Disconnect from the WiFi network
int wifi_disconnect(void)
{
    int ret;
    struct net_if *iface = net_if_get_default();

    ret = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);

    return ret;
}
