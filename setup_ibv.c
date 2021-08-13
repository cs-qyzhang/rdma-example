#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <infiniband/verbs.h>
#include "util/ibv_print_info.h"
#include "setup_ibv.h"

const int SERVER_SOCK_PORT = 32214;
const char* SERVER_ADDR = "192.168.2.2";

void print_peer_info(struct peer_info* peer) {
    printf("-------------------- peer info --------------------\n");
    printf("peer lid: %#hx\n", peer->lid);
    printf("peer qpn: %#x\n", peer->qpn);
    printf("peer psn: %#x\n", peer->psn);
    printf("peer gid: %#llx %#llx\n", peer->gid.global.subnet_prefix, peer->gid.global.interface_id);
}

// server listen on socket
static int server_exchange_peer_info(struct peer_info* mine, struct peer_info* peer) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    if (inet_pton(AF_INET, SERVER_ADDR, &serv_addr.sin_addr)<=0) {
        perror("inet_pton");
        close(sockfd);
        return -1;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_SOCK_PORT);

    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(sockfd);
        return -1;
    }
    if (listen(sockfd, 5)) {
        perror("listen");
        close(sockfd);
        return -1;
    }

    int connfd = accept(sockfd, NULL, NULL);
    if (connfd < 0) {
        perror("accpet");
        close(sockfd);
        return -1;
    }
    printf("conneted to client\n");
    char buffer[sizeof(*peer) + 10];
    int recv_sz = read(connfd, buffer, sizeof(*peer) + 10);
    if (recv_sz != sizeof(*peer)) {
        fprintf(stderr, "recv_sz != sizeof(struct peer_info)!\n");
        close(connfd);
        close(sockfd);
        return -1;
    }
    memcpy((char*)peer, buffer, sizeof(*peer));

    int send_sz = send(connfd, (char*)mine, sizeof(*mine), 0);
    if (send_sz != sizeof(*mine)) {
        fprintf(stderr, "send_sz != sizeof(struct peer_info)!\n");
        close(connfd);
        close(sockfd);
        return -1;
    }

    close(connfd);
    close(sockfd);
    return 0;
}

static int client_exchange_peer_info(struct peer_info* mine, struct peer_info* peer) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket error!\n");
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;

    serv_addr.sin_port = htons(SERVER_SOCK_PORT);
    if (inet_pton(AF_INET, SERVER_ADDR, &serv_addr.sin_addr)<=0) {
        perror("inet_pton error occured\n");
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
       perror("Error : Connect Failed \n");
       close(sockfd);
       return 1;
    }
    printf("connectted to server\n");

    int send_sz = send(sockfd, (char*)mine, sizeof(*mine), 0);
    if (send_sz != sizeof(*mine)) {
        fprintf(stderr, "send_sz != sizeof(struct peer_info)!\n");
        close(sockfd);
        return -1;
    }

    char buffer[sizeof(*peer) + 10];
    int recv_sz = read(sockfd, buffer, sizeof(*peer) + 10);
    if (recv_sz != sizeof(*peer)) {
        fprintf(stderr, "recv_sz != sizeof(struct peer_info)!\n");
        close(sockfd);
        return -1;
    }
    memcpy((char*)peer, buffer, sizeof(*peer));

    close(sockfd);
    return 0;
}

static int exchange_info_with_peer(struct peer_info* mine, struct peer_info* peer,
                            int is_server) {
    return is_server ? server_exchange_peer_info(mine, peer) :
                       client_exchange_peer_info(mine, peer);
}

int setup_rdma(struct rdma_context* ctx, int is_server) {
    assert(ctx->mr_length != 0UL);
    assert(ctx->alloc_mr != NULL);
    assert(ctx->free_mr != NULL);

    // get device list
    ctx->device_list = ibv_get_device_list(&ctx->num_dev);
    if (ctx->device_list == NULL) {
        perror("ibv_get_device_list");
        return -1;
    }
    if (ctx->num_dev < 1) {
        fprintf(stderr, "ibv_get_device_list: no device avaiable!\n");
        return -1;
    }
    print_device_list(ctx->device_list, ctx->num_dev);

    // open device
    ctx->device = is_server ? ctx->device_list[0] : ctx->device_list[1];
    ctx->dev_context = ibv_open_device(ctx->device);
    if (ctx->dev_context == NULL) {
        perror("ibv_open_device");
        goto free_device_list;
    }

    // query device attr
    if (ibv_query_device(ctx->dev_context, &ctx->device_attr)) {
        perror("ibv_query_device");
        goto close_device;
    }
    print_device_attr(&ctx->device_attr, is_server ? 0 : 1);

    // query port attr
    if (ibv_query_port(ctx->dev_context, 1, &ctx->port_attr)) {
        perror("ibv_query_port");
        goto close_device;
    }
    print_port_attr(&ctx->port_attr, 1);

    // query gid
    for (int i = 0; i < ctx->port_attr.gid_tbl_len; ++i) {
        union ibv_gid gid;
        if (ibv_query_gid(ctx->dev_context, 1, i, &gid)) {
            perror("ibv_query_gid");
            goto close_device;
        }
        if (gid.global.subnet_prefix != 0 || gid.global.interface_id != 0)
            print_gid(&gid, i);
        // index 0 is port's gid
        if (i == 0)
            ctx->gid = gid;
    }

    // query pkey
    for (int i = 0; i < ctx->port_attr.pkey_tbl_len; ++i) {
        uint16_t pkey;
        if (ibv_query_pkey(ctx->dev_context, 1, i, &pkey)) {
            perror("ibv_query_pkey");
            goto close_device;
        }
        print_pkey(pkey, i);
    }

    // alloc protection domain
    ctx->pd = ibv_alloc_pd(ctx->dev_context);
    if (ctx->pd == NULL) {
        perror("ibv_alloc_pd");
        goto close_device;
    }

    // create completion queue
    ctx->cq = ibv_create_cq(ctx->dev_context, 5, NULL, NULL, 0);
    if (ctx->cq == NULL) {
        perror("ibv_create_cq");
        goto dealloc_pd;
    }

    // alloc mr
    if (ctx->alloc_mr(&ctx->mr_addr, ctx->mr_length, ctx->mr_arg)) {
        fprintf(stderr, "alloc_mr error!\n");
        goto dealloc_pd;
    }

    // register memory region
    ctx->mr = ibv_reg_mr(ctx->pd, ctx->mr_addr, ctx->mr_length,
                         IBV_ACCESS_LOCAL_WRITE |
                         IBV_ACCESS_REMOTE_READ);
    if (ctx->mr == NULL) {
        perror("ibv_reg_mr");
        goto free_mr;
    }

    // create queue pair
    memset(&ctx->qp_init_attr, 0, sizeof(ctx->qp_init_attr));
    ctx->qp_init_attr.qp_type = IBV_QPT_RC;
    ctx->qp_init_attr.send_cq = ctx->cq;
    ctx->qp_init_attr.recv_cq = ctx->cq;
    // if sq_sig_all is 1, every ibv_post_send() will generate a CQE and can be
    // polled by ibv_poll_cq(); if sig_sig_all is 0, then only the ibv_post_send()
    // which set send_flags to IBV_SEND_SIGNALED can be polled by ibv_poll_cq()
    ctx->qp_init_attr.sq_sig_all = 1;
    ctx->qp_init_attr.cap.max_send_wr = 5;
    ctx->qp_init_attr.cap.max_recv_wr = 5;
    ctx->qp_init_attr.cap.max_send_sge = 5;
    ctx->qp_init_attr.cap.max_recv_sge = 5;
    ctx->qp_init_attr.cap.max_inline_data = 512; // maximum 512?
    ctx->qp = ibv_create_qp(ctx->pd, &ctx->qp_init_attr);
    if (ctx->qp == NULL) {
        perror("ibv_create_qp");
        goto dereg_mr;
    }
    print_qp_init_attr(&ctx->qp_init_attr);

    // RESET to INIT: bind port, specify access control
    memset(&ctx->qp_attr, 0, sizeof(ctx->qp_attr));
    ctx->qp_attr.qp_state = IBV_QPS_INIT;
    ctx->qp_attr.pkey_index = 0;
    ctx->qp_attr.port_num = 1;
    ctx->qp_attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE |
                                   IBV_ACCESS_REMOTE_READ;
    if (ibv_modify_qp(ctx->qp, &ctx->qp_attr, IBV_QP_STATE |
                                              IBV_QP_PKEY_INDEX |
                                              IBV_QP_PORT |
                                              IBV_QP_ACCESS_FLAGS)) {
        perror("ibv_modify_qp RESET to INIT");
        goto destroy_qp;
    }

    // retrive remote qp's information
    struct peer_info mine;
    mine.gid = ctx->gid;
    mine.lid = ctx->port_attr.lid;
    mine.qpn = ctx->qp->qp_num;
    mine.psn = is_server ? 0x20000 : 0x10;
    if (exchange_info_with_peer(&mine, &ctx->peer_info, is_server)) {
        goto destroy_qp;
    }
    print_peer_info(&ctx->peer_info);

    // INIT to RTR
    ctx->qp_attr.qp_state = IBV_QPS_RTR;
    ctx->qp_attr.path_mtu = IBV_MTU_1024;
    ctx->qp_attr.dest_qp_num = ctx->peer_info.qpn;
    ctx->qp_attr.rq_psn = ctx->peer_info.psn;
    ctx->qp_attr.max_dest_rd_atomic = 1;
    ctx->qp_attr.min_rnr_timer = 12;
    ctx->qp_attr.ah_attr.dlid = 0;
    ctx->qp_attr.ah_attr.port_num = 1;
    // RoCE must have GRH
    ctx->qp_attr.ah_attr.is_global = 1;
    ctx->qp_attr.ah_attr.grh.dgid = ctx->peer_info.gid;
    ctx->qp_attr.ah_attr.grh.hop_limit = 1;
    ctx->qp_attr.ah_attr.grh.sgid_index = 0;
    if (ibv_modify_qp(ctx->qp, &ctx->qp_attr, IBV_QP_STATE |
                                              IBV_QP_PATH_MTU |
                                              IBV_QP_DEST_QPN |
                                              IBV_QP_RQ_PSN |
                                              IBV_QP_MAX_DEST_RD_ATOMIC |
                                              IBV_QP_MIN_RNR_TIMER |
                                              IBV_QP_AV)) {
        perror("ibv_modify_qp INIT to RTR");
        goto destroy_qp;
    }

    // RTR to RTS
    ctx->qp_attr.qp_state = IBV_QPS_RTS;
    ctx->qp_attr.timeout = 14;
    ctx->qp_attr.retry_cnt = 7;
    ctx->qp_attr.rnr_retry = 7;
    ctx->qp_attr.sq_psn = mine.psn;
    ctx->qp_attr.max_rd_atomic = 1;
    if (ibv_modify_qp(ctx->qp, &ctx->qp_attr, IBV_QP_STATE |
                                              IBV_QP_TIMEOUT |
                                              IBV_QP_RETRY_CNT |
                                              IBV_QP_RNR_RETRY |
                                              IBV_QP_SQ_PSN |
                                              IBV_QP_MAX_QP_RD_ATOMIC)) {
        perror("ibv_modify_qp RTR to RTS");
        goto destroy_qp;
    }

    return 0;

destroy_qp:
    ibv_destroy_qp(ctx->qp);
dereg_mr:
    ibv_dereg_mr(ctx->mr);
free_mr:
    ctx->free_mr(ctx->mr_addr, ctx->mr_length, ctx->mr_arg);
destroy_cq:
    ibv_destroy_cq(ctx->cq);
dealloc_pd:
    ibv_dealloc_pd(ctx->pd);
close_device:
    ibv_close_device(ctx->dev_context);
free_device_list:
    ibv_free_device_list(ctx->device_list);

    return -1;
}

void fini_rdma(struct rdma_context* ctx) {
    ibv_destroy_qp(ctx->qp);
    ibv_dereg_mr(ctx->mr);
    ctx->free_mr(ctx->mr_addr, ctx->mr_length, ctx->mr_arg);
    ibv_destroy_cq(ctx->cq);
    ibv_dealloc_pd(ctx->pd);
    ibv_close_device(ctx->dev_context);
    ibv_free_device_list(ctx->device_list);
}