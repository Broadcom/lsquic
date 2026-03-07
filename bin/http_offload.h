/* SPDX-License-Identifier: MIT */
/*
 * QUIC offload manager for http_server
 * Copyright Broadcom 2025
 */
#ifndef HTTP_OFFLOAD_H
#define HTTP_OFFLOAD_H

#include "lsquic.h"

#ifdef HAVE_BNXT_EN_DRIVER

#include <linux/bnxt_quic_usr_include.h>

void http_offload_add_flow(lsquic_conn_t *conn, int sock_fd, const char *if_name,
                         enum lsquic_offload_direction offload_dir);
void http_offload_delete_flow(lsquic_conn_t *conn, int sock_fd, const char *if_name);
int  http_offload_flush_flows(int sock_fd, const char *if_name);

int print_bnxt_quic_connection_info(struct bnxt_quic_connection_info *info);
int populate_bnxt_quic_connection_info(struct lsquic_conn *lconn,
                                     struct bnxt_quic_connection_info *info);
extern struct bnxt_quic_connection_info bnxt_info;

#else /* !HAVE_BNXT_EN_DRIVER */

static inline void
http_offload_add_flow(lsquic_conn_t *conn, int sock_fd, const char *if_name,
                      enum lsquic_offload_direction offload_dir)
{
    (void)conn; (void)sock_fd; (void)if_name; (void)offload_dir;
}

static inline void
http_offload_delete_flow(lsquic_conn_t *conn, int sock_fd, const char *if_name)
{
    (void)conn; (void)sock_fd; (void)if_name;
}

static inline int
http_offload_flush_flows(int sock_fd, const char *if_name)
{
    (void)sock_fd; (void)if_name;
    return 0;
}

#endif /* HAVE_BNXT_EN_DRIVER */

#endif /* HTTP_OFFLOAD_H */
