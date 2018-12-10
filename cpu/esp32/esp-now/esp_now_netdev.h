/*
 * Copyright (C) 2018 Gunar Schorcht
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
 * @brief       Netdev interface for the ESP-NOW WiFi P2P protocol
 *
 * @author      Gunar Schorcht <gunar@schorcht.net>
 * @author      Timo Rothenpieler <timo.rothenpieler@uni-bremen.de>
 */

#ifndef ESP_NOW_NETDEV_H
#define ESP_NOW_NETDEV_H

#include "net/netdev.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Maximum raw packet size that can be used with ESP-NOW (including headers)
 */
#define ESP_NOW_MAX_SIZE_RAW (250)

/**
 * @brief   Length of ESP-NOW addresses
 */
#define ESP_NOW_ADDR_LEN ETHERNET_ADDR_LEN

/**
 * @brief   Size of non-data header elements in ESP-NOW packet
 */
#define ESP_NOW_HEADER_LENGTH (sizeof(esp_now_pkt_hdr_t))

/**
 * @brief   Maximum packet size that can be used with ESP-NOW
 */
#define ESP_NOW_MAX_SIZE (ESP_NOW_MAX_SIZE_RAW - ESP_NOW_HEADER_LENGTH)

/**
 * @brief   Reference to the netdev device driver struct
 */
extern const netdev_driver_t esp_now_driver;

/**
 * @brief   Header with neccesary flags for ESP-NOW packets
 */
typedef struct __attribute__((packed))
{
    uint8_t flags; /**< Flags */
} esp_now_pkt_hdr_t;

/**
 * @brief   Packed ESP-NOW packet buffer
 */
typedef struct __attribute__((packed))
{
    esp_now_pkt_hdr_t hdr; /**< Header */
    uint8_t data[ESP_NOW_MAX_SIZE]; /**< L3 data */
} esp_now_pkt_buf_t;

/**
 * @brief   struct holding esp_now packet + metadata
 */
typedef struct
{
    esp_now_pkt_buf_t buf; /**< packet data */

    uint8_t len; /**< number of bytes in buf (including header) */
    uint8_t mac[ESP_NOW_ADDR_LEN]; /**< l2 packet source/destination address (depending on context) */
} esp_now_pkt_t;

/**
 * @brief   Device descriptor for ESP-NOW devices
 */
typedef struct
{
    netdev_t netdev;                 /**< netdev parent struct */

    uint8_t addr[ESP_NOW_ADDR_LEN];  /**< device addr (MAC address) */

    esp_now_pkt_t rx_pkt;            /**< receive packet */

    gnrc_netif_t* netif;             /**< reference to the corresponding netif */

#ifdef MODULE_GNRC
    gnrc_nettype_t proto;            /**< protocol for upper layer */
#endif

    uint8_t peers_all;               /**< number of peers reachable */
    uint8_t peers_enc;               /**< number of encrypted peers */

    mutex_t dev_lock;                /**< device is already in use */

} esp_now_netdev_t;

/**
 * @brief netdev <-> esp_npw glue code initialization function
 *
 * @return          NULL on error, pointer to esp_now_netdev on success
 */
esp_now_netdev_t *netdev_esp_now_setup(void);

#ifdef __cplusplus
}
#endif

#endif /* ESP_NOW_NETDEV_H */
/** @} */
