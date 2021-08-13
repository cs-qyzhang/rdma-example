#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <infiniband/verbs.h>
#include "util/ibv_print_info.h"
#include "setup_ibv.h"

static int alloc_mr(void** addr, uint64_t length, void* arg) {
    *addr = malloc(length);
    if (*addr == NULL) {
        perror("malloc");
        return -1;
    }
    return 0;
}

static int free_mr(void* addr, uint64_t length, void* arg) {
    free(addr);
    return 0;
}

int main(int argc, char** argv) {
    int is_server = argc > 1;

    struct rdma_context ctx;
    ctx.mr_length = 1024;
    ctx.alloc_mr = alloc_mr;
    ctx.free_mr = free_mr;
    if (setup_rdma(&ctx, is_server)) {
        fprintf(stderr, "cannot setup rdma");
        return -1;
    }

    // connection established
    if (is_server) {
        // server post receive
        struct ibv_sge recv_wr_sge;
        memset(&recv_wr_sge, 0, sizeof(recv_wr_sge));
        memset(ctx.mr->addr, 0, 1024);
        recv_wr_sge.addr = (uint64_t)ctx.mr->addr;
        recv_wr_sge.length = 128;
        recv_wr_sge.lkey = ctx.mr->lkey;

        struct ibv_recv_wr recv_wr;
        memset(&recv_wr, 0, sizeof(recv_wr));
        recv_wr.wr_id = 0;
        recv_wr.sg_list = &recv_wr_sge;
        recv_wr.num_sge = 1;

        struct ibv_recv_wr* bad_recv_wr;
        if (ibv_post_recv(ctx.qp, &recv_wr, &bad_recv_wr)) {
            if (bad_recv_wr == &recv_wr)
                fprintf(stderr, "ibv_post_recv get bad recv wr");
            perror("ibv_post_recv");
            fini_rdma(&ctx);
            return -1;
        }

        // poll cqe
        struct ibv_wc wc;
        int polled_num;
        do {
            polled_num = ibv_poll_cq(ctx.cq, 1, &wc);
        } while (polled_num == 0);
        if (polled_num < 0) {
            perror("ibv_poll_cq");
            fini_rdma(&ctx);
            return -1;
        }
        assert(polled_num == 1);
        printf("polled\n");
        print_wc(&wc);
        printf("%s\n", (char*)ctx.mr->addr);
    } else {
        // client post send
        char* data = "Hello, world!";
        memcpy(ctx.mr->addr, data, strlen(data) + 1);
        struct ibv_sge send_sge;
        send_sge.addr = (uint64_t)ctx.mr->addr;
        send_sge.length = strlen(data) + 1;
        send_sge.lkey = ctx.mr->lkey;

        struct ibv_send_wr send_wr;
        memset(&send_wr, 0, sizeof(send_wr));
        send_wr.opcode = IBV_WR_SEND;
        send_wr.wr_id = 0;
        send_wr.num_sge = 1;
        send_wr.sg_list = &send_sge;

        struct ibv_send_wr* bad_send_wr;
        if (ibv_post_send(ctx.qp, &send_wr, &bad_send_wr)) {
            if (bad_send_wr == &send_wr)
                fprintf(stderr, "ibv_post_send get bad send wr");
            perror("ibv_post_send");
            fini_rdma(&ctx);
            return -1;
        }
        if (bad_send_wr == &send_wr) {
            fprintf(stderr, "ibv_post_send get bad send wr");
            fini_rdma(&ctx);
            return -1;
        }

        // poll cqe
        struct ibv_wc wc;
        int polled_num = 0;
        do {
            polled_num = ibv_poll_cq(ctx.cq, 1, &wc);
        } while (polled_num == 0);
        if (polled_num < 0) {
            perror("ibv_poll_cq");
            fini_rdma(&ctx);
            return -1;
        }
        assert(polled_num == 1);
        printf("polled\n");
        print_wc(&wc);
    }

    fini_rdma(&ctx);

    return 0;
}
