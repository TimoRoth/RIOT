/*
 * Copyright (C) 2018 Timo Rothenpieler
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_esp32_esp_now
 * @{
 *
 * @file
 * @brief       Netif interface for the ESP-NOW WiFi P2P protocol
 *
 * @author Timo Rothenpieler <timo.rothenpieler@uni-bremen.de>
 */

#include <assert.h>

#include <sys/uio.h>

#include "net/netdev.h"
#include "net/gnrc.h"
#include "esp_now_params.h"
#include "esp_now_netdev.h"
#include "esp_now_netif.h"
#include "net/gnrc/netif.h"
#include "od.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

static int _send(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
{
    esp_now_pkt_t esp_now_pkt;
    netdev_t *dev = netif->dev;

    assert(pkt != NULL);

    gnrc_netif_hdr_t *netif_hdr;
    gnrc_pktsnip_t *payload;

    payload = pkt->next;

    if (pkt->type != GNRC_NETTYPE_NETIF) {
        DEBUG("gnrc_esp_now: First header was not generic netif header\n");
        gnrc_pktbuf_release(pkt);
        return -EBADMSG;
    }

    netif_hdr = (gnrc_netif_hdr_t*)pkt->data;

    if (netif_hdr->flags & (GNRC_NETIF_HDR_FLAGS_BROADCAST | GNRC_NETIF_HDR_FLAGS_MULTICAST)) {
        /* ESP-NOW does not support multicast, always broadcast */
        memset(esp_now_pkt.mac, 0xff, ESP_NOW_ADDR_LEN);
    } else if (netif_hdr->dst_l2addr_len == ESP_NOW_ADDR_LEN) {
        memcpy(esp_now_pkt.mac, gnrc_netif_hdr_get_dst_addr(netif_hdr), ESP_NOW_ADDR_LEN);
    } else {
        DEBUG("gnrc_esp_now: destination address had unexpected format"
              "(flags=%d, dst_l2addr_len=%d)\n", netif_hdr->flags, netif_hdr->dst_l2addr_len);
        return -EBADMSG;
    }

    switch (payload->type) {
#ifdef MODULE_GNRC_SIXLOWPAN
        case GNRC_NETTYPE_SIXLOWPAN:
            esp_now_pkt.buf.hdr.flags = 1;
            break;
#endif
        default:
            esp_now_pkt.buf.hdr.flags = 0;
    }

    iolist_t iolist = {
        .iol_base = (char*)&esp_now_pkt,
        .iol_len = sizeof(esp_now_pkt_t)
    };

    unsigned payload_len = 0;
    uint8_t *pos = esp_now_pkt.buf.data;

    while (payload) {
        payload_len += payload->size;

        if (payload_len > ESP_NOW_MAX_SIZE) {
            DEBUG("gnrc_esp_now: payload length exceeds maximum(%u>%u)\n",
                  payload_len, ESP_NOW_MAX_SIZE);
            gnrc_pktbuf_release(pkt);
            return -EBADMSG;
        }

        memcpy(pos, payload->data, payload->size);
        pos += payload->size;
        payload = payload->next;
    }

    /* pkt has been copied into esp_now_pkt, we're done with it. */
    gnrc_pktbuf_release(pkt);

    esp_now_pkt.len = ESP_NOW_HEADER_LENGTH + (uint8_t)payload_len;

    DEBUG("gnrc_esp_now: sending packet to %02x:%02x:%02x:%02x:%02x:%02x with size %u\n",
            esp_now_pkt.mac[0], esp_now_pkt.mac[1], esp_now_pkt.mac[2],
            esp_now_pkt.mac[3], esp_now_pkt.mac[4], esp_now_pkt.mac[5],
            (unsigned)payload_len);
#if defined(MODULE_OD) && ENABLE_DEBUG
    od_hex_dump(esp_now_pkt.buf.data, payload_len, OD_WIDTH_DEFAULT);
#endif

    return dev->driver->send(dev, &iolist);
}

static gnrc_pktsnip_t *_recv(gnrc_netif_t *netif)
{
    netdev_t *dev = netif->dev;
    esp_now_netdev_t *esp_now = (esp_now_netdev_t*)dev;

    /*
     * buf == &esp_now_netdev->rx_pkt is a special case, avoiding a memcpy and 250 bytes
     * extra stack usage. It leaves the responsibility of resetting rx_pkt.len to 0 to us.
     */
    int recv_res = dev->driver->recv(dev, &esp_now->rx_pkt, sizeof(esp_now->rx_pkt), NULL);
    if (recv_res <= 0) {
        DEBUG("gnrc_esp_now: failed receiving packet: %d\n", recv_res);
        return NULL;
    }

    int nettype;
    switch (esp_now->rx_pkt.buf.hdr.flags) {
#ifdef MODULE_GNRC_SIXLOWPAN
        case 1:
            nettype = GNRC_NETTYPE_SIXLOWPAN;
            break;
#endif
        default:
            nettype = GNRC_NETTYPE_UNDEF;
    }

    /* copy packet payload into pktbuf */
    unsigned pkt_len = esp_now->rx_pkt.len - ESP_NOW_HEADER_LENGTH;
    gnrc_pktsnip_t *pkt = gnrc_pktbuf_add(NULL, esp_now->rx_pkt.buf.data, pkt_len, nettype);

    if(!pkt) {
        DEBUG("gnrc_esp_now: _recv: cannot allocate pktsnip.\n");
        return NULL;
    }

    gnrc_pktsnip_t *netif_hdr;
    netif_hdr = gnrc_pktbuf_add(
                    NULL,
                    NULL,
                    sizeof(gnrc_netif_hdr_t) + 2 * ESP_NOW_ADDR_LEN,
                    GNRC_NETTYPE_NETIF);
    if (!netif_hdr) {
        DEBUG("gnrc_esp_now: no space left in packet buffer\n");
        gnrc_pktbuf_release(pkt);
        return NULL;
    }

    gnrc_netif_hdr_init(netif_hdr->data, ESP_NOW_ADDR_LEN, ESP_NOW_ADDR_LEN);
    gnrc_netif_hdr_set_src_addr(netif_hdr->data, esp_now->rx_pkt.mac, ESP_NOW_ADDR_LEN);
    gnrc_netif_hdr_set_dst_addr(netif_hdr->data, esp_now->addr, ESP_NOW_ADDR_LEN);

    ((gnrc_netif_hdr_t *)netif_hdr->data)->if_pid = thread_getpid();

    DEBUG("gnrc_esp_now: received packet from %02x:%02x:%02x:%02x:%02x:%02x of length %u\n",
            esp_now->rx_pkt.mac[0], esp_now->rx_pkt.mac[1], esp_now->rx_pkt.mac[2],
            esp_now->rx_pkt.mac[3], esp_now->rx_pkt.mac[4], esp_now->rx_pkt.mac[5],
            pkt_len);
#if defined(MODULE_OD) && ENABLE_DEBUG
    od_hex_dump(esp_now->rx_pkt.buf.data, pkt_len, OD_WIDTH_DEFAULT);
#endif

    pkt->next = netif_hdr;
    esp_now->rx_pkt.len = 0;

    return pkt;
}

static const gnrc_netif_ops_t _esp_now_ops = {
    .send = _send,
    .recv = _recv,
    .get = gnrc_netif_get_from_netdev,
    .set = gnrc_netif_set_from_netdev,
};

gnrc_netif_t *gnrc_netif_esp_now_create(char *stack, int stacksize, char priority,
                                        char *name, netdev_t *dev)
{
    return gnrc_netif_create(stack, stacksize, priority, name, dev, &_esp_now_ops);
}

/* device thread stack */
static char _esp_now_stack[ESP_NOW_STACKSIZE];

void auto_init_esp_now(void)
{
    LOG_TAG_INFO("esp_now", "initializing ESP-NOW device\n");

    esp_now_netdev_t *esp_now_dev = netdev_esp_now_setup();
    if (!esp_now_dev) {
        LOG_ERROR("[auto_init_netif] error initializing esp_now\n");
    } else {
        esp_now_dev->netif =
            gnrc_netif_esp_now_create(_esp_now_stack,
                                      ESP_NOW_STACKSIZE, ESP_NOW_PRIO,
                                      "net-esp-now",
                                      &esp_now_dev->netdev);
    }
}

/** @} */
