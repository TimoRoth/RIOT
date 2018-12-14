/*
 * Copyright (C) 2018 Timo Rothenpieler
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup cpu_esp32_esp_now
 * @{
 *
 * @file
 * @brief   ESP-NOW adaption for @ref net_gnrc_netif
 *
 * @author  Timo Rothenpieler <timo.rothenpieler@uni-bremen.de>
 */
#ifndef ESP_NOW_NETIF_H
#define ESP_NOW_NETIF_H

#include "net/gnrc/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

gnrc_netif_t *gnrc_netif_esp_now_create(char *stack, int stacksize, char priority,
                                        char *name, netdev_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* ESP_NOW_NETIF_H */
/** @} */
