/* SPDX-License-Identifier: MIT */
/*
 * QUIC offload manager for http_server
 * Copyright Broadcom 2025
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <openssl/ssl.h>
#include <linux/tls.h>

#include "lsquic.h"
#include "http_offload.h"

#include "../src/liblsquic/lsquic_int_types.h"
#include "../src/liblsquic/lsquic_hash.h"
#include "../src/liblsquic/lsquic_conn.h"
#include "../src/liblsquic/lsquic_logger.h"
#include "../src/liblsquic/lsquic_util.h"


char hex_dump_to_buffer[0x1000];
int print_bnxt_quic_connection_info(struct bnxt_quic_connection_info *info)
{
    LSQ_INFO("----- bnxt_quic_connection_info -----");
    LSQ_INFO("cipher=%u, key_mask=0x%x", info->cipher, info->key_mask);
    LSQ_INFO("tx_conn_id=%llu, rx_conn_id=%llu",
        info->tx_conn_id, info->rx_conn_id);
    lsquic_hexdump(info->tx_data_key, BNXT_MAX_KEY_SIZE, hex_dump_to_buffer, sizeof(hex_dump_to_buffer));
    LSQ_INFO("tx_data_key=\n%s", hex_dump_to_buffer);
    lsquic_hexdump(info->tx_hdr_key, BNXT_MAX_KEY_SIZE, hex_dump_to_buffer, sizeof(hex_dump_to_buffer));
    LSQ_INFO("tx_hdr_key=\n%s", hex_dump_to_buffer);
    lsquic_hexdump(info->tx_iv, BNXT_IV_SIZE, hex_dump_to_buffer, sizeof(hex_dump_to_buffer));
    LSQ_INFO("tx_iv=\n%s", hex_dump_to_buffer);
    lsquic_hexdump(info->rx_data_key, BNXT_MAX_KEY_SIZE, hex_dump_to_buffer, sizeof(hex_dump_to_buffer));
    LSQ_INFO("rx_data_key=\n%s", hex_dump_to_buffer);
    lsquic_hexdump(info->rx_hdr_key, BNXT_MAX_KEY_SIZE, hex_dump_to_buffer, sizeof(hex_dump_to_buffer));
    LSQ_INFO("rx_hdr_key=\n%s", hex_dump_to_buffer);
    lsquic_hexdump(info->rx_iv, BNXT_IV_SIZE, hex_dump_to_buffer, sizeof(hex_dump_to_buffer));
    LSQ_INFO("rx_iv=\n%s", hex_dump_to_buffer);
    LSQ_INFO("daddr=%s, dport=%u, saddr=%s, sport=%u    dst_conn_id_width=%u, pkt_number=%llu",
        inet_ntoa(((struct sockaddr_in *)&info->flow.daddr)->sin_addr), ntohs(((struct sockaddr_in *)&info->flow.daddr)->sin_port),
        inet_ntoa(((struct sockaddr_in *)&info->flow.saddr)->sin_addr), ntohs(((struct sockaddr_in *)&info->flow.saddr)->sin_port),
        info->dst_conn_id_width, info->pkt_number);
    LSQ_INFO("--------------------------------");
    return 0;
}

/**
 * populate_flow_info - Fill in the common 5-tuple from connection path
 * @lconn: QUIC connection
 * @flow_info: destination flow info structure
 *
 * Copies the source and destination addresses from the connection's
 * network path into the flow_info structure.
 */
static int populate_flow_info(struct lsquic_conn *lconn,
                              struct bnxt_quic_flow_info *flow_info)
{
    const struct network_path *path;

    if (!lconn || !flow_info)
        return -EINVAL;

    path = lconn->cn_if->ci_get_path(lconn, NULL);
    if (!path)
        return -EINVAL;

    memcpy(&flow_info->saddr, NP_LOCAL_SA(path), sizeof(struct sockaddr));
    memcpy(&flow_info->daddr, NP_PEER_SA(path), sizeof(struct sockaddr));

    return 0;
}

static int populate_addr_and_connection_id(struct lsquic_conn *lconn,
                                           struct bnxt_quic_connection_info *info)
{
    const lsquic_cid_t *scid, *dcid;
    const struct network_path *path;
    int rc;

    rc = populate_flow_info(lconn, &info->flow);
    if (rc)
        return rc;

    path = lconn->cn_if->ci_get_path(lconn, NULL);
    scid = lsquic_conn_id(lconn);
    dcid = &path->np_dcid;
    if (lconn->cn_flags & LSCONN_SERVER)
    {
        memcpy(&info->tx_conn_id, dcid->buf, sizeof(info->tx_conn_id));
        memcpy(&info->rx_conn_id, scid->buf, sizeof(info->rx_conn_id));
    }
    else
    {
        memcpy(&info->tx_conn_id, scid->buf, sizeof(info->tx_conn_id));
        memcpy(&info->rx_conn_id, dcid->buf, sizeof(info->rx_conn_id));
    }
    info->dst_conn_id_width = dcid->len;

    LSQ_DEBUG("daddr=%s, dport=%u, saddr=%s, sport=%u dst_conn_id_width=%u",
        inet_ntoa(((struct sockaddr_in *)&info->flow.daddr)->sin_addr), ntohs(((struct sockaddr_in *)&info->flow.daddr)->sin_port),
        inet_ntoa(((struct sockaddr_in *)&info->flow.saddr)->sin_addr), ntohs(((struct sockaddr_in *)&info->flow.saddr)->sin_port),
        info->dst_conn_id_width);
    return 0;
}

int populate_bnxt_quic_connection_info(struct lsquic_conn *lconn,
                                     struct bnxt_quic_connection_info *info)
{
    struct lsquic_offload_crypto_info crypto_info;
    int rc;

    rc = populate_addr_and_connection_id(lconn, info);
    if (rc != 0)
        return rc;

    rc = lsquic_conn_get_offload_crypto_info(lconn, &crypto_info, 0 /* key_phase */);
    if (rc)
        return rc;

    info->cipher = crypto_info.cipher;
    switch (crypto_info.cipher) {
        case SSL_CIPHER_AES_128_GCM_SHA256:
            info->cipher = TLS_CIPHER_AES_GCM_128;
            break;
        case SSL_CIPHER_AES_256_GCM_SHA384:
            info->cipher = TLS_CIPHER_AES_GCM_256;
            break;
        default:
            LSQ_ERROR("populate_bnxt_quic_connection_info: negotiated_cipher=0x%x not supported", crypto_info.cipher);
            return -EINVAL;
    }

    memcpy(info->tx_data_key, crypto_info.tx_data_key, crypto_info.key_len);
    memcpy(info->tx_iv, crypto_info.tx_iv, LSQUIC_OFFLOAD_IV_LEN);
    memcpy(info->tx_hdr_key, crypto_info.tx_hdr_key, crypto_info.key_len);

    memcpy(info->rx_data_key, crypto_info.rx_data_key, crypto_info.key_len);
    memcpy(info->rx_iv, crypto_info.rx_iv, LSQUIC_OFFLOAD_IV_LEN);
    memcpy(info->rx_hdr_key, crypto_info.rx_hdr_key, crypto_info.key_len);

    info->pkt_number = lsquic_conn_get_last_sent_packno(lconn);

    LSQ_DEBUG("populate_bnxt_quic_connection_info: pkt_number=%llu", info->pkt_number);

    return 0;
}


void http_offload_add_flow(lsquic_conn_t *conn, int sock_fd, const char *if_name,
                         enum lsquic_offload_direction offload_dir)
{
    struct ifreq ifr;
    struct bnxt_quic_connection_info bnxt_info;
    int rc;
    const lsquic_cid_t *scid, *dcid;
    const struct network_path *path;

    if (offload_dir != LSQUIC_OFFLOAD_TX)
        return;

    if (sock_fd < 0 || !if_name[0])
    {
        LSQ_ERROR("http_offload_add_flow: sock_fd=%d, if_name=%s, offload_dir=%d", sock_fd, if_name, offload_dir);
        return;
    }

    scid = lsquic_conn_id(conn);
    path = conn->cn_if->ci_get_path(conn, NULL);
    if (!path)
    {
        LSQ_ERROR("Cannot offload: could not get network path for connection.");
        return;
    }
    dcid = &path->np_dcid;

    /*
     * HW OFFLOAD VALIDATION:
     * The offload hardware has strict limitations on supported CID lengths.
     *  - The peer's CID (used for our TX path) must be 0 or 8 bytes.
     *  - Our own CID (used for our RX path) must be 8 bytes.
     * If these conditions are not met, the connection cannot be offloaded
     * and must be handled in software.
     */
    if ((dcid->len != 0 && dcid->len != 8) || scid->len != 8)
    {
        LSQ_WARN("Cannot offload connection: CID lengths not supported by hardware "
                 "(peer_cid_len: %u, local_cid_len: %u). Supported peer_len: 0 or 8; "
                 "local_len: 8. Connection will run in software.",
                 dcid->len, scid->len);
        return;
    }

    memset(&bnxt_info, 0, sizeof(bnxt_info));
    rc = populate_bnxt_quic_connection_info(conn, &bnxt_info);
    if (rc != 0)
    {
        LSQ_ERROR("Failed to populate bnxt_quic_connection_info: %d", rc);
        return;
    }

    /* Map lsquic offload direction to BNXT key_mask.
     * Currently only TX offload is supported by hardware.
     * When RX support is added, LSQUIC_OFFLOAD_RX would map to
     * BNXT_QUIC_KEY_RX_PHASE_0 and LSQUIC_OFFLOAD_BOTH would map to
     * BNXT_QUIC_KEY_TX_PHASE_0 | BNXT_QUIC_KEY_RX_PHASE_0.
     */
    bnxt_info.key_mask = BNXT_QUIC_KEY_TX_PHASE_0;
    print_bnxt_quic_connection_info(&bnxt_info);

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, IFNAMSIZ - 1);
    ifr.ifr_data = (char *)&bnxt_info;
    /* print the ifr data */
    LSQ_INFO("ifr.ifr_name=%s", ifr.ifr_name);
    rc = ioctl(sock_fd, SIOCDEVQUICFLOWADD, &ifr);
    if (rc < 0)
    {
        LSQ_ERROR("ioctl(SIOCDEVQUICFLOWADD) failed: %s", strerror(errno));
    }
    else
    {
        lsquic_conn_set_offload_status(conn, offload_dir);
        LSQ_INFO("Successfully added QUIC flow for offload %s", if_name);
        LSQ_DEBUG("daddr=%s, dport=%u, saddr=%s, sport=%u    dst_conn_id_width=%u, pkt_number=%llu",
            inet_ntoa(((struct sockaddr_in *)&bnxt_info.flow.daddr)->sin_addr), ntohs(((struct sockaddr_in *)&bnxt_info.flow.daddr)->sin_port),
            inet_ntoa(((struct sockaddr_in *)&bnxt_info.flow.saddr)->sin_addr), ntohs(((struct sockaddr_in *)&bnxt_info.flow.saddr)->sin_port),
            bnxt_info.dst_conn_id_width, bnxt_info.pkt_number);
    }
}

int http_offload_flush_flows(int sock_fd, const char *if_name)
{
    struct ifreq ifr;
    int rc;

    if (sock_fd < 0 || !if_name || !if_name[0])
    {
        LSQ_ERROR("http_offload_flush_flows: invalid sock_fd=%d or if_name", sock_fd);
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, IFNAMSIZ - 1);

    rc = ioctl(sock_fd, SIOCDEVQUICFLOWFLUSH, &ifr);
    if (rc < 0)
    {
        LSQ_ERROR("ioctl(SIOCDEVQUICFLOWFLUSH) failed on %s: %s",
                  if_name, strerror(errno));
        return -1;
    }

    LSQ_NOTICE("Flushed all QUIC offload flows on %s", if_name);
    return 0;
}

void http_offload_delete_flow(lsquic_conn_t *conn, int sock_fd, const char *if_name)
{
    struct ifreq ifr;
    struct bnxt_quic_flow_del_info del_info;
    int rc;

    if (lsquic_conn_get_offload_status(conn) == LSQUIC_OFFLOAD_NONE)
        return;

    memset(&del_info, 0, sizeof(del_info));
    rc = populate_flow_info(conn, &del_info.flow);
    if (rc != 0)
    {
        LSQ_ERROR("Failed to populate flow info for delete: %d", rc);
        return;
    }

    /* Delete all TX keys (both key phases) for this flow */
    del_info.key_mask = BNXT_QUIC_KEY_TX_ALL;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, IFNAMSIZ - 1);
    ifr.ifr_data = (char *)&del_info;

    rc = ioctl(sock_fd, SIOCDEVQUICFLOWDEL, &ifr);
    if (rc < 0)
    {
        LSQ_ERROR("ioctl(SIOCDEVQUICFLOWDEL) failed: %s", strerror(errno));
    }
    else
    {
        LSQ_INFO("Successfully deleted QUIC flow from offload");
        lsquic_conn_set_offload_status(conn, LSQUIC_OFFLOAD_NONE);
    }
}
