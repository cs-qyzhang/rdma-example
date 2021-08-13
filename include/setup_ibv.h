#pragma once

#include <infiniband/verbs.h>

// information needed to exchange with peer to establish connection
struct peer_info {
    uint16_t lid;
    uint32_t qpn;
    uint32_t psn;
    union ibv_gid gid;
};

struct rdma_context {
    struct ibv_device_attr  device_attr;
    struct ibv_port_attr    port_attr;
    struct ibv_qp_init_attr qp_init_attr;
    struct ibv_qp_attr      qp_attr;
    union ibv_gid           gid;
    struct peer_info        peer_info;
    struct ibv_device**     device_list;
    struct ibv_device*      device;
    struct ibv_cq*          cq;
    struct ibv_qp*          qp;
    struct ibv_context*     dev_context;
    struct ibv_pd*          pd;
    struct ibv_mr*          mr;
    void*                   mr_addr;
    uint64_t                mr_length;
    void*                   mr_arg;
    int (*alloc_mr)(void** addr, uint64_t length, void* arg);
    int (*free_mr)(void* addr, uint64_t length, void* arg);
    int                     num_dev;
};

int setup_rdma(struct rdma_context* ctx, int is_server);
void fini_rdma(struct rdma_context* ctx);
void print_peer_info(struct peer_info* peer);