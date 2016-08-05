/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <string.h>
#include "os/os.h"
#include "bsp/bsp.h"

/* BLE */
#include "nimble/ble.h"
#include "nimble/hci_transport.h"
#include "nimble/hci_common.h"
#include "host/host_hci.h"
#include "host/ble_hs.h"
#include "controller/ble_ll.h"
#include "controller/ble_ll_hci.h"
#include "controller/ble_ll_conn.h"
#include "controller/ble_ll_scan.h"
#include "controller/ble_ll_adv.h"

/* XXX: An app should not include private headers from a library.  The bletest
 * app uses some of nimble's internal details for logging.
 */
#include "../src/ble_hci_util_priv.h"
#include "../src/ble_hs_priv.h"
#include "bletest_priv.h"

extern uint16_t g_bletest_ltk_reply_handle;

void
bletest_send_conn_update(uint16_t handle)
{
    int rc;
    struct hci_conn_update hcu;

    hcu.conn_latency = 4;
    hcu.supervision_timeout = 2000;
    hcu.conn_itvl_min = 1000;
    hcu.conn_itvl_max = 1000;
    hcu.handle = handle;
    hcu.min_ce_len = 4;
    hcu.max_ce_len = 4;

    rc = host_hci_cmd_le_conn_update(&hcu);
    assert(rc == 0);
}

#if (BLE_LL_CFG_FEAT_LE_ENCRYPTION == 1)
void
bletest_ltk_req_reply(uint16_t handle)
{
    g_bletest_ltk_reply_handle = handle;
}

int
bletest_send_ltk_req_neg_reply(uint16_t handle)
{
    int rc;
    uint8_t *dst;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + sizeof(uint16_t)];
    uint16_t ack_conn_handle;
    uint8_t rsplen;

    dst = buf;
    host_hci_write_hdr(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_LT_KEY_REQ_NEG_REPLY,
                       sizeof(uint16_t), dst);
    dst += BLE_HCI_CMD_HDR_LEN;

    htole16(dst, handle);
    rc = ble_hci_cmd_tx(buf, &ack_conn_handle, 2, &rsplen);
    if (rc == 0) {
        if (rsplen != 2) {
            rc = -1;
        }
    }

    return rc;
}

int
bletest_send_ltk_req_reply(uint16_t handle)
{
    struct hci_lt_key_req_reply hkr;
    uint16_t ack_conn_handle;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_LT_KEY_REQ_REPLY_LEN];
    uint8_t ack_params_len;
    int rc;

    hkr.conn_handle = handle;
    swap_buf(hkr.long_term_key, (uint8_t *)g_bletest_LTK, 16);

    host_hci_cmd_build_le_lt_key_req_reply(&hkr, buf, sizeof buf);
    rc = ble_hci_cmd_tx(buf, &ack_conn_handle, sizeof ack_conn_handle,
                        &ack_params_len);
    if (rc != 0) {
        return rc;
    }
    if (ack_params_len != BLE_HCI_LT_KEY_REQ_REPLY_ACK_PARAM_LEN - 1) {
        return -1;
    }

    ack_conn_handle = TOFROMLE16(ack_conn_handle);
    if (ack_conn_handle != handle) {
        return -1;
    }
    return 0;
}
#endif

int
bletest_hci_reset_ctlr(void)
{
    uint8_t buf[BLE_HCI_CMD_HDR_LEN];

    host_hci_write_hdr(BLE_HCI_OGF_CTLR_BASEBAND, BLE_HCI_OCF_CB_RESET, 0, buf);
    return ble_hci_cmd_tx(buf, NULL, 0, NULL);
}

int
bletest_hci_rd_bd_addr(void)
{
    int rc;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN];
    uint8_t rspbuf[BLE_DEV_ADDR_LEN];
    uint8_t rsplen;

    host_hci_write_hdr(BLE_HCI_OGF_INFO_PARAMS, BLE_HCI_OCF_IP_RD_BD_ADDR, 0,
                       buf);
    rc = ble_hci_cmd_tx(buf, rspbuf, BLE_DEV_ADDR_LEN, &rsplen);
    if (rc != 0) {
        return rc;
    }

    if (rsplen != BLE_DEV_ADDR_LEN) {
        return BLE_HS_ECONTROLLER;
    }

    return rc;
}

#if (BLE_LL_CFG_FEAT_LE_ENCRYPTION == 1)
int
bletest_hci_le_encrypt(uint8_t *key, uint8_t *pt)
{
    int rc;
    uint8_t *dst;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_LE_ENCRYPT_LEN];
    uint8_t rspbuf[16];
    uint8_t rsplen;

    dst = buf;
    host_hci_write_hdr(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_ENCRYPT,
                       BLE_HCI_LE_ENCRYPT_LEN, dst);
    dst += BLE_HCI_CMD_HDR_LEN;

    swap_buf(dst, key, BLE_ENC_BLOCK_SIZE);
    swap_buf(dst + BLE_ENC_BLOCK_SIZE, pt, BLE_ENC_BLOCK_SIZE);
    rc = ble_hci_cmd_tx(buf, rspbuf, 16, &rsplen);
    if (rc != 0) {
        return rc;
    }

    if (rsplen != 16) {
        return BLE_HS_ECONTROLLER;
    }
    return rc;
}
#endif

int
bletest_hci_le_set_datalen(uint16_t handle, uint16_t txoctets, uint16_t txtime)
{
    int rc;
    uint8_t *dst;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_SET_DATALEN_LEN];
    uint8_t rspbuf[2];
    uint8_t rsplen;

    dst = buf;
    host_hci_write_hdr(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_SET_DATA_LEN,
                       BLE_HCI_SET_DATALEN_LEN, dst);
    dst += BLE_HCI_CMD_HDR_LEN;

    htole16(dst, handle);
    htole16(dst + 2, txoctets);
    htole16(dst + 4, txtime);
    rc = ble_hci_cmd_tx(buf, rspbuf, 2, &rsplen);
    if (rc != 0) {
        return rc;
    }

    if (rsplen != 2) {
        return BLE_HS_ECONTROLLER;
    }

    return rc;
}

int
bletest_hci_le_write_sugg_datalen(uint16_t txoctets, uint16_t txtime)
{
    uint8_t *dst;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_WR_SUGG_DATALEN_LEN];

    dst = buf;
    host_hci_write_hdr(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_WR_SUGG_DEF_DATA_LEN,
                       BLE_HCI_WR_SUGG_DATALEN_LEN, dst);
    dst += BLE_HCI_CMD_HDR_LEN;

    htole16(dst, txoctets);
    htole16(dst + 2, txtime);
    return ble_hci_cmd_tx(buf, NULL, 0, NULL);
}

int
bletest_hci_le_rd_sugg_datalen(void)
{
    int rc;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN];
    uint8_t rspbuf[BLE_HCI_RD_SUGG_DATALEN_RSPLEN];
    uint8_t rsplen;

    host_hci_write_hdr(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_RD_SUGG_DEF_DATA_LEN, 0,
                       buf);

    rc = ble_hci_cmd_tx(buf, rspbuf, BLE_HCI_RD_SUGG_DATALEN_RSPLEN, &rsplen);
    if (rc != 0) {
        return rc;
    }

    if (rsplen != BLE_HCI_RD_SUGG_DATALEN_RSPLEN) {
        return BLE_HS_ECONTROLLER;
    }

    return 0;
}

int
bletest_hci_rd_local_version(void)
{
    int rc;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN];
    uint8_t rspbuf[BLE_HCI_RD_LOC_VER_INFO_RSPLEN];
    uint8_t rsplen;

    host_hci_write_hdr(BLE_HCI_OGF_INFO_PARAMS, BLE_HCI_OCF_IP_RD_LOCAL_VER, 0,
                       buf);
    rc = ble_hci_cmd_tx(buf, rspbuf, BLE_HCI_RD_LOC_VER_INFO_RSPLEN, &rsplen);
    if (rc != 0) {
        return rc;
    }

    if (rsplen != BLE_HCI_RD_LOC_VER_INFO_RSPLEN) {
        return BLE_HS_ECONTROLLER;
    }
    return rc;
}

int
bletest_hci_rd_local_feat(void)
{
    int rc;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN];
    uint8_t rspbuf[BLE_HCI_RD_LOC_SUPP_FEAT_RSPLEN];
    uint8_t rsplen;

    host_hci_write_hdr(BLE_HCI_OGF_INFO_PARAMS, BLE_HCI_OCF_IP_RD_LOC_SUPP_FEAT,
                       0, buf);
    rc = ble_hci_cmd_tx(buf, rspbuf, BLE_HCI_RD_LOC_SUPP_FEAT_RSPLEN, &rsplen);
    if (rc != 0) {
        return rc;
    }

    if (rsplen != BLE_HCI_RD_LOC_SUPP_FEAT_RSPLEN) {
        return BLE_HS_ECONTROLLER;
    }
    return rc;
}

int
bletest_hci_rd_local_supp_cmd(void)
{
    int rc;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN];
    uint8_t rspbuf[BLE_HCI_RD_LOC_SUPP_CMD_RSPLEN];
    uint8_t rsplen;

    host_hci_write_hdr(BLE_HCI_OGF_INFO_PARAMS, BLE_HCI_OCF_IP_RD_LOC_SUPP_CMD,
                       0, buf);
    rc = ble_hci_cmd_tx(buf, rspbuf, BLE_HCI_RD_LOC_SUPP_CMD_RSPLEN, &rsplen);
    if (rc != 0) {
        return rc;
    }

    if (rsplen != BLE_HCI_RD_LOC_SUPP_CMD_RSPLEN) {
        return BLE_HS_ECONTROLLER;
    }
    return rc;
}

/**
 * Read supported states
 *
 * OGF = 0x08 (LE)
 * OCF = 0x001C
 *
 * @return int
 */
int
bletest_hci_le_read_supp_states(void)
{
    int rc;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN];
    uint8_t rspbuf[BLE_HCI_RD_SUPP_STATES_RSPLEN];
    uint8_t rsplen;

    host_hci_write_hdr(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_RD_SUPP_STATES, 0, buf);
    rc = ble_hci_cmd_tx(buf, rspbuf, BLE_HCI_RD_SUPP_STATES_RSPLEN, &rsplen);
    if (rc != 0) {
        return rc;
    }

    if (rsplen != BLE_HCI_RD_SUPP_STATES_RSPLEN) {
        return BLE_HS_ECONTROLLER;
    }
    return rc;
}

int
bletest_hci_le_rd_max_datalen(void)
{
    int rc;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN];
    uint8_t rspbuf[BLE_HCI_RD_MAX_DATALEN_RSPLEN];
    uint8_t rsplen;

    host_hci_write_hdr(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_RD_MAX_DATA_LEN, 0, buf);
    rc = ble_hci_cmd_tx(buf, rspbuf, BLE_HCI_RD_MAX_DATALEN_RSPLEN, &rsplen);
    if (rc != 0) {
        return rc;
    }

    if (rsplen != BLE_HCI_RD_MAX_DATALEN_RSPLEN) {
        return BLE_HS_ECONTROLLER;
    }
    return rc;
}

int
bletest_hci_le_set_adv_data(uint8_t *data, uint8_t len)
{
    int rc;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_SET_ADV_DATA_LEN];

    rc = host_hci_cmd_build_le_set_adv_data(data, len, buf, sizeof buf);
    assert(rc == 0);
    return ble_hci_cmd_tx_empty_ack(buf);
}

#if (BLE_LL_CFG_FEAT_LE_ENCRYPTION == 1)
int
bletest_hci_le_start_encrypt(struct hci_start_encrypt *cmd)
{
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_LE_START_ENCRYPT_LEN];

    host_hci_cmd_build_le_start_encrypt(cmd, buf, sizeof buf);
    return ble_hci_cmd_tx_empty_ack(buf);
}
#endif

int
bletest_hci_le_read_rem_used_feat(uint16_t handle)
{
    uint8_t *dst;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_CONN_RD_REM_FEAT_LEN];

    dst = buf;
    host_hci_write_hdr(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_RD_REM_FEAT,
                       BLE_HCI_CONN_RD_REM_FEAT_LEN, dst);
    dst += BLE_HCI_CMD_HDR_LEN;

    htole16(dst, handle);
    return ble_hci_cmd_tx(buf, NULL, 0, NULL);
}

int
bletest_hci_le_set_adv_params(struct hci_adv_params *adv)
{
    int rc;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_SET_ADV_PARAM_LEN];

    rc = host_hci_cmd_build_le_set_adv_params(adv, buf, sizeof buf);
    if (!rc) {
        rc = ble_hci_cmd_tx_empty_ack(buf);
    }
    return rc;
}

int
bletest_hci_le_set_rand_addr(uint8_t *addr)
{
    uint8_t *dst;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_SET_DATALEN_LEN];

    dst = buf;
    host_hci_write_hdr(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_SET_RAND_ADDR,
                       BLE_DEV_ADDR_LEN, dst);
    dst += BLE_HCI_CMD_HDR_LEN;

    memcpy(dst, addr, BLE_DEV_ADDR_LEN);
    return ble_hci_cmd_tx(buf, NULL, 0, NULL);
}

int
bletest_hci_rd_rem_version(uint16_t handle)
{
    uint8_t *dst;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + sizeof(uint16_t)];

    dst = buf;
    host_hci_write_hdr(BLE_HCI_OGF_LINK_CTRL, BLE_HCI_OCF_RD_REM_VER_INFO,
                       sizeof(uint16_t), dst);
    dst += BLE_HCI_CMD_HDR_LEN;

    htole16(dst, handle);
    return ble_hci_cmd_tx(buf, NULL, 0, NULL);
}

int
bletest_hci_le_set_host_chan_class(uint8_t *chanmap)
{
    uint8_t *dst;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_SET_HOST_CHAN_CLASS_LEN];

    dst = buf;
    host_hci_write_hdr(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_SET_HOST_CHAN_CLASS,
                       BLE_HCI_SET_HOST_CHAN_CLASS_LEN, dst);
    dst += BLE_HCI_CMD_HDR_LEN;

    memcpy(dst, chanmap, BLE_HCI_SET_HOST_CHAN_CLASS_LEN);
    return ble_hci_cmd_tx(buf, NULL, 0, NULL);
}

int
bletest_hci_le_rd_chanmap(uint16_t handle)
{
    int rc;
    uint8_t *dst;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_RD_CHANMAP_LEN];
    uint8_t rspbuf[BLE_HCI_RD_CHANMAP_RSP_LEN];
    uint8_t rsplen;

    dst = buf;
    host_hci_write_hdr(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_RD_CHAN_MAP,
                       BLE_HCI_RD_CHANMAP_LEN, dst);
    dst += BLE_HCI_CMD_HDR_LEN;

    htole16(dst, handle);
    rc = ble_hci_cmd_tx(buf, rspbuf, BLE_HCI_RD_CHANMAP_RSP_LEN, &rsplen);
    if (rc != 0) {
        return rc;
    }

    if (rsplen != BLE_HCI_RD_CHANMAP_RSP_LEN) {
        return BLE_HS_ECONTROLLER;
    }

    return rc;
}

int
bletest_hci_le_set_adv_enable(uint8_t enable)
{
    uint8_t *dst;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_SET_ADV_ENABLE_LEN];

    dst = buf;
    host_hci_write_hdr(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_SET_ADV_ENABLE,
                       BLE_HCI_SET_ADV_ENABLE_LEN, dst);
    dst += BLE_HCI_CMD_HDR_LEN;

    dst[0] = enable;
    return ble_hci_cmd_tx(buf, NULL, 0, NULL);
}

int
bletest_hci_le_set_event_mask(uint64_t event_mask)
{
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_SET_LE_EVENT_MASK_LEN];

    host_hci_cmd_build_le_set_event_mask(event_mask, buf, sizeof buf);
    return ble_hci_cmd_tx_empty_ack(buf);
}

int
bletest_hci_set_event_mask(uint64_t event_mask)
{
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_SET_EVENT_MASK_LEN];

    host_hci_cmd_build_set_event_mask(event_mask, buf, sizeof buf);
    return ble_hci_cmd_tx_empty_ack(buf);
}

int
bletest_hci_le_set_scan_rsp_data(uint8_t *data, uint8_t len)
{
    int rc;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_SET_SCAN_RSP_DATA_LEN];

    rc = host_hci_cmd_build_le_set_scan_rsp_data(data, len, buf, sizeof buf);
    assert(rc == 0);
    return ble_hci_cmd_tx_empty_ack(buf);
}

int
bletest_hci_cmd_le_set_scan_params(uint8_t scan_type, uint16_t scan_itvl,
                                   uint16_t scan_window, uint8_t own_addr_type,
                                   uint8_t filter_policy) {
    int rc;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_SET_SCAN_PARAM_LEN];

    rc = host_hci_cmd_build_le_set_scan_params(scan_type, scan_itvl,
                                               scan_window, own_addr_type,
                                               filter_policy, buf, sizeof buf);
    if (!rc) {
        rc = ble_hci_cmd_tx_empty_ack(buf);
    }
    return rc;
}

int
bletest_hci_le_add_to_whitelist(uint8_t *addr, uint8_t addr_type)
{
    int rc;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_SET_SCAN_PARAM_LEN];

    rc = host_hci_cmd_build_le_add_to_whitelist(addr, addr_type, buf,
                                                sizeof buf);
    if (!rc) {
        rc = ble_hci_cmd_tx_empty_ack(buf);
    }
    return rc;
}

int
bletest_hci_le_set_scan_enable(uint8_t enable, uint8_t filter_dups)
{
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_SET_SCAN_ENABLE_LEN];

    host_hci_cmd_build_le_set_scan_enable(enable, filter_dups, buf, sizeof buf);
    return ble_hci_cmd_tx_empty_ack(buf);
}

int
bletest_hci_le_create_connection(struct hci_create_conn *hcc)
{
    int rc;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_CREATE_CONN_LEN];

    rc = host_hci_cmd_build_le_create_connection(hcc, buf, sizeof buf);
    if (!rc) {
        rc = ble_hci_cmd_tx_empty_ack(buf);
    }
    return rc;
}

int
bletest_hci_le_add_resolv_list(uint8_t *local_irk, uint8_t *peer_irk,
                               uint8_t *peer_ident_addr, uint8_t addr_type)
{
    int rc;
    struct hci_add_dev_to_resolving_list padd;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_ADD_TO_RESOLV_LIST_LEN];

    padd.addr_type = addr_type;
    memcpy(padd.addr, peer_ident_addr, BLE_DEV_ADDR_LEN);
    swap_buf(padd.local_irk, local_irk, 16);
    swap_buf(padd.peer_irk, peer_irk, 16);
    rc = host_hci_cmd_build_add_to_resolv_list(&padd, buf, sizeof buf);
    if (!rc) {
        rc = ble_hci_cmd_tx_empty_ack(buf);
    }
    return rc;
}

int
bletest_hci_le_enable_resolv_list(uint8_t enable)
{
    int rc;
    uint8_t buf[BLE_HCI_CMD_HDR_LEN + BLE_HCI_SET_ADDR_RESOL_ENA_LEN];


    rc = host_hci_cmd_build_set_addr_res_en(enable, buf, sizeof buf);
    if (!rc) {
        rc = ble_hci_cmd_tx_empty_ack(buf);
    }
    return rc;
}

