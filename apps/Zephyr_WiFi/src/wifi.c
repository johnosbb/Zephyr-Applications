/*
 * Copyright (c) 2023 Craig Peacock.
 * Copyright (c) 2017 ARM Ltd.
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_ip.h>

#include <errno.h>
#include <string.h>

#include "ei_config.h"   /* SSID, WIFI_PASS */
#include "http_get.h"
#include "ping.h"

static K_SEM_DEFINE(wifi_connected, 0, 1);
static K_SEM_DEFINE(ipv4_address_obtained, 0, 1);

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;

/* Forward declarations for handlers used before definition */
static void handle_wifi_connect_result(struct net_mgmt_event_callback *cb);
static void handle_wifi_disconnect_result(struct net_mgmt_event_callback *cb);
static void handle_ipv4_result(struct net_if *iface);

/* ------------------------------- Scan support -------------------------------- */
static K_SEM_DEFINE(scan_done, 0, 1);

static bool ipv4_is_configured(struct net_if *iface)
{
    struct net_if_ipv4 *ipv4 = iface->config.ip.ipv4;

    for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
        uint8_t type = ipv4->unicast[i].ipv4.addr_type;

        if (type == NET_ADDR_DHCP || type == NET_ADDR_MANUAL) {
            if (!net_ipv4_is_addr_unspecified(&ipv4->unicast[i].ipv4.address.in_addr)) {
                return true;
            }
        }
    }
    return false;
}


static void handle_wifi_scan_result(struct net_mgmt_event_callback *cb)
{
    const struct wifi_scan_result *entry =
        (const struct wifi_scan_result *)cb->info;

    printk("[SCAN] SSID:%s  CH:%u  SEC:%s  RSSI:%d\n",
           entry->ssid,
           entry->channel,
           wifi_security_txt(entry->security),
           entry->rssi);
}

static void handle_wifi_scan_done(struct net_mgmt_event_callback *cb)
{
    const struct wifi_status *status =
        (const struct wifi_status *)cb->info;
    printk("[SCAN] done (status=%d)\n", status ? status->status : 0);
    k_sem_give(&scan_done);
}

static void wifi_scan_and_print(void)
{
    struct net_if *iface = net_if_get_default();
    int ret;

    printk("Starting WiFi scan...\n");
    ret = net_mgmt(NET_REQUEST_WIFI_SCAN, iface, NULL, 0);
    if (ret) {
        printk("Scan request failed (%d)\n", ret);
        return;
    }
    if (k_sem_take(&scan_done, K_SECONDS(10)) != 0) {
        printk("Scan timed out\n");
    }
}

/* --------------------------- Event handler (single) --------------------------- */
static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                    uint64_t mgmt_event,
                                    struct net_if *iface)
{
    switch (mgmt_event) {
    case NET_EVENT_WIFI_SCAN_RESULT:
        handle_wifi_scan_result(cb);
        break;
    case NET_EVENT_WIFI_SCAN_DONE:
        handle_wifi_scan_done(cb);
        break;
    case NET_EVENT_WIFI_CONNECT_RESULT:
        handle_wifi_connect_result(cb);
        break;
    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        handle_wifi_disconnect_result(cb);
        break;
    case NET_EVENT_IPV4_ADDR_ADD:
        handle_ipv4_result(iface);
        break;
    default:
        break;
    }
}

/* -------------------------- Original helper handlers ------------------------- */
static void handle_wifi_connect_result(struct net_mgmt_event_callback *cb)
{
    const struct wifi_status *status = (const struct wifi_status *)cb->info;

    if (status->status) {
        printk("Connection request failed (%d)\n", status->status);
    } else {
        printk("Connection request sent\n");
        k_sem_give(&wifi_connected);
    }
}

static void handle_wifi_disconnect_result(struct net_mgmt_event_callback *cb)
{
    const struct wifi_status *status = (const struct wifi_status *)cb->info;

    if (status->status) {
        printk("Disconnection request (%d)\n", status->status);
    } else {
        printk("Disconnected\n");
        k_sem_take(&wifi_connected, K_NO_WAIT);
    }
}

static void print_ipv4_info_now(struct net_if *iface)
{
    for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
        char buf[NET_IPV4_ADDR_LEN];
        uint8_t type = iface->config.ip.ipv4->unicast[i].ipv4.addr_type;

        if (type != NET_ADDR_DHCP && type != NET_ADDR_MANUAL) {
            continue;
        }

        printk("IPv4 address (%s): %s\n",
               (type == NET_ADDR_DHCP) ? "DHCP" : "Static",
               net_addr_ntop(AF_INET,
                             &iface->config.ip.ipv4->unicast[i].ipv4.address.in_addr,
                             buf, sizeof(buf)));

        printk("Subnet: %s\n",
               net_addr_ntop(AF_INET,
                             &iface->config.ip.ipv4->unicast[i].netmask,
                             buf, sizeof(buf)));

        printk("Router: %s\n",
               net_addr_ntop(AF_INET,
                             &iface->config.ip.ipv4->gw,
                             buf, sizeof(buf)));
    }
}



static void handle_ipv4_result(struct net_if *iface)
{
    for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
        char buf[NET_IPV4_ADDR_LEN];
        uint8_t type = iface->config.ip.ipv4->unicast[i].ipv4.addr_type;

        if (type != NET_ADDR_DHCP && type != NET_ADDR_MANUAL) {
            continue;
        }

        printk("IPv4 address (%s): %s\n",
               (type == NET_ADDR_DHCP) ? "DHCP" : "Static",
               net_addr_ntop(AF_INET,
                             &iface->config.ip.ipv4->unicast[i].ipv4.address.in_addr,
                             buf, sizeof(buf)));

        printk("Subnet: %s\n",
               net_addr_ntop(AF_INET,
                             &iface->config.ip.ipv4->unicast[i].netmask,
                             buf, sizeof(buf)));

        printk("Router: %s\n",
               net_addr_ntop(AF_INET,
                             &iface->config.ip.ipv4->gw,
                             buf, sizeof(buf)));
    }

    k_sem_give(&ipv4_address_obtained);
}

/* --------------------------------- API calls -------------------------------- */
void wifi_connect(void)
{
    struct net_if *iface = net_if_get_default();

    struct wifi_connect_req_params wifi_params = {0};

    wifi_params.ssid = WIFI_SSID;
    wifi_params.psk = WIFI_PASS;
    wifi_params.ssid_length = strlen(WIFI_SSID);
    wifi_params.psk_length = strlen(WIFI_PASS);
    wifi_params.channel = WIFI_CHANNEL_ANY;
    wifi_params.security = WIFI_SECURITY_TYPE_PSK;
    wifi_params.band = WIFI_FREQ_BAND_2_4_GHZ;
    wifi_params.mfp = WIFI_MFP_OPTIONAL;

    printk("Wifi_Connect:Connecting to target SSID: %s\n", wifi_params.ssid);

    if (net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &wifi_params,
                 sizeof(struct wifi_connect_req_params))) {
        printk("WiFi Connection Request Failed\n");
    }
}

void wifi_status(void)
{
    struct net_if *iface = net_if_get_default();
    struct wifi_iface_status status = {0};

    if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status,
                 sizeof(struct wifi_iface_status))) {
        printk("WiFi Status Request Failed\n");
    }

    printk("\n");

    if (status.state >= WIFI_STATE_ASSOCIATED) {
        printk("SSID: %-32s\n", status.ssid);
        printk("Band: %s\n", wifi_band_txt(status.band));
        printk("Channel: %d\n", status.channel);
        printk("Security: %s\n", wifi_security_txt(status.security));
        printk("RSSI: %d\n", status.rssi);
    }
}

void wifi_disconnect(void)
{
    struct net_if *iface = net_if_get_default();

    if (net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0)) {
        printk("WiFi Disconnection Request Failed\n");
    }
}

/* ---------------------------------- main() ---------------------------------- */
int main(void)
{
    int sock;
    struct net_if *iface = net_if_get_default();

    printk("WiFi Example\nBoard: %s\n", CONFIG_BOARD);

    net_mgmt_init_event_callback(
        &wifi_cb,
        wifi_mgmt_event_handler,
        NET_EVENT_WIFI_CONNECT_RESULT |
        NET_EVENT_WIFI_DISCONNECT_RESULT |
        NET_EVENT_WIFI_SCAN_RESULT |
        NET_EVENT_WIFI_SCAN_DONE);

    net_mgmt_init_event_callback(&ipv4_cb, wifi_mgmt_event_handler,
                                 NET_EVENT_IPV4_ADDR_ADD);

    net_mgmt_add_event_callback(&wifi_cb);
    net_mgmt_add_event_callback(&ipv4_cb);

    wifi_scan_and_print();
    wifi_connect();

    k_sem_take(&wifi_connected, K_FOREVER);
    wifi_status();

    print_ipv4_info_now(iface);

    /* Only wait for IPv4 event if we don't already have one (DHCP case) */
    if (!ipv4_is_configured(iface)) {
        k_sem_take(&ipv4_address_obtained, K_FOREVER);
        print_ipv4_info_now(iface); /* print once it arrives */
    }

    printk("Ready...\n\n");

    ping("8.8.8.8", 4);

    printk("\nLooking up IP addresses:\n");
    struct zsock_addrinfo *res;
    nslookup("iot.beyondlogic.org", &res);
    print_addrinfo_results(&res);

    printk("\nConnecting to HTTP Server:\n");
    sock = connect_socket(&res, 80);
    http_get(sock, "iot.beyondlogic.org", "/LoremIpsum.txt");
    zsock_close(sock);

    return 0;
}
