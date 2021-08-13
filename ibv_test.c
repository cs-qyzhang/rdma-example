#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <infiniband/verbs.h>
#include "util/ibv_print_info.h"

const int MR_SIZE = 1024;
const int SERVER_SOCK_PORT = 32214;
const char* SERVER_ADDR = "192.168.2.2";

// information needed to exchange with peer to establish connection
struct peer_info {
    uint16_t lid;
    uint32_t qpn;
    uint32_t psn;
    union ibv_gid gid;
};

void print_peer_info(struct peer_info* peer) {
    printf("-------------------- peer info --------------------\n");
    printf("peer lid: %#hx\n", peer->lid);
    printf("peer qpn: %#x\n", peer->qpn);
    printf("peer psn: %#x\n", peer->psn);
    printf("peer gid: %#llx %#llx\n", peer->gid.global.subnet_prefix, peer->gid.global.interface_id);
}

// server listen on socket
int server_exchange_peer_info(struct peer_info* mine, struct peer_info* peer) {
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

int client_exchange_peer_info(struct peer_info* mine, struct peer_info* peer) {
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

int exchange_info_with_peer(struct peer_info* mine, struct peer_info* peer,
                            int is_server) {
    return is_server ? server_exchange_peer_info(mine, peer) :
                       client_exchange_peer_info(mine, peer);
}

int main(int argc, char** argv) {
    int return_code = 0;

    int is_server = 0;
    if (argc > 1)
        is_server = 1;

    // get device list
    int num_dev = 0;
    struct ibv_device** device_list = ibv_get_device_list(&num_dev);
    if (device_list == NULL) {
        perror("ibv_get_device_list");
        return -1;
    }
    if (num_dev < 1) {
        fprintf(stderr, "ibv_get_device_list: no device avaiable!\n");
        return -1;
    }
    print_device_list(device_list, num_dev);

    // open device
    struct ibv_device* device = is_server ? device_list[0] : device_list[1];
    struct ibv_context* context = ibv_open_device(device);
    if (context == NULL) {
        perror("ibv_open_device");
        return_code = -1;
        goto free_device_list;
    }

    // query device attr
    struct ibv_device_attr device_attr;
    if (ibv_query_device(context, &device_attr)) {
        perror("ibv_query_device");
        return_code = -1;
        goto close_device;
    }
    print_device_attr(&device_attr, is_server ? 0 : 1);

    // query port attr
    struct ibv_port_attr port_attr;
    if (ibv_query_port(context, 1, &port_attr)) {
        perror("ibv_query_port");
        return_code = -1;
        goto close_device;
    }
    print_port_attr(&port_attr, 1);

    // query gid
    union ibv_gid gid;
    for (int i = 0; i < port_attr.gid_tbl_len; ++i) {
        union ibv_gid tmp_gid;
        if (ibv_query_gid(context, 1, i, &tmp_gid)) {
            perror("ibv_query_gid");
            return_code = -1;
            goto close_device;
        }
        if (tmp_gid.global.subnet_prefix != 0 || tmp_gid.global.interface_id != 0)
            print_gid(&tmp_gid, i);
        // index 0 is port's gid
        if (i == 0)
            gid = tmp_gid;
    }

    // query pkey
    for (int i = 0; i < port_attr.pkey_tbl_len; ++i) {
        uint16_t pkey;
        if (ibv_query_pkey(context, 1, i, &pkey)) {
            perror("ibv_query_pkey");
            return_code = -1;
            goto close_device;
        }
        print_pkey(pkey, i);
    }

    // alloc protection domain
    struct ibv_pd* pd = ibv_alloc_pd(context);
    if (pd == NULL) {
        perror("ibv_alloc_pd");
        return_code = -1;
        goto close_device;
    }

    // create completion queue
    struct ibv_cq* cq = ibv_create_cq(context, 5, NULL, NULL, 0);
    if (cq == NULL) {
        perror("ibv_create_cq");
        return_code = -1;
        goto dealloc_pd;
    }

    // register memory region
    void* mr_addr = malloc(MR_SIZE);
    if (mr_addr == NULL) {
        perror("malloc");
        return_code = -1;
        goto destroy_cq;
    }
    struct ibv_mr* mr = ibv_reg_mr(pd, mr_addr, MR_SIZE,
                                   IBV_ACCESS_LOCAL_WRITE |
                                   IBV_ACCESS_REMOTE_READ);
    if (mr == NULL) {
        perror("ibv_reg_mr");
        return_code = -1;
        goto free_mr;
    }

    // create queue pair
    struct ibv_qp_init_attr qp_init_attr;
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.send_cq = cq;
    qp_init_attr.recv_cq = cq;
    // if sq_sig_all is 1, every ibv_post_send() will generate a CQE and can be
    // polled by ibv_poll_cq(); if sig_sig_all is 0, then only the ibv_post_send()
    // which set send_flags to IBV_SEND_SIGNALED can be polled by ibv_poll_cq()
    qp_init_attr.sq_sig_all = 1;
    qp_init_attr.cap.max_send_wr = 5;
    qp_init_attr.cap.max_recv_wr = 5;
    qp_init_attr.cap.max_send_sge = 5;
    qp_init_attr.cap.max_recv_sge = 5;
    qp_init_attr.cap.max_inline_data = 512; // maximum 512?
    struct ibv_qp* qp = ibv_create_qp(pd, &qp_init_attr);
    if (qp == NULL) {
        perror("ibv_create_qp");
        return_code = -1;
        goto dereg_mr;
    }
    print_qp_init_attr(&qp_init_attr);

    // RESET to INIT: bind port, specify access control
    struct ibv_qp_attr qp_attr;
    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.qp_state = IBV_QPS_INIT;
    qp_attr.pkey_index = 0;
    qp_attr.port_num = 1;
    qp_attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ;
    if (ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE |
                                    IBV_QP_PKEY_INDEX |
                                    IBV_QP_PORT |
                                    IBV_QP_ACCESS_FLAGS)) {
        perror("ibv_modify_qp RESET to INIT");
        return_code = -1;
        goto destroy_qp;
    }

    // retrive remote qp's information
    struct peer_info mine, peer;
    mine.gid = gid;
    mine.lid = port_attr.lid;
    mine.qpn = qp->qp_num;
    mine.psn = is_server ? 0x20000 : 0x10;
    if (exchange_info_with_peer(&mine, &peer, is_server)) {
        return_code = -1;
        goto destroy_qp;
    }
    print_peer_info(&peer);

    // INIT to RTR
    qp_attr.qp_state = IBV_QPS_RTR;
    qp_attr.path_mtu = IBV_MTU_1024;
    qp_attr.dest_qp_num = peer.qpn;
    qp_attr.rq_psn = peer.psn;
    qp_attr.max_dest_rd_atomic = 1;
    qp_attr.min_rnr_timer = 12;
    qp_attr.ah_attr.dlid = 0;
    qp_attr.ah_attr.port_num = 1;
    // RoCE must have GRH
    qp_attr.ah_attr.is_global = 1;
    qp_attr.ah_attr.grh.dgid = peer.gid;
    qp_attr.ah_attr.grh.hop_limit = 1;
    qp_attr.ah_attr.grh.sgid_index = 0;
    if (ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE |
                                    IBV_QP_PATH_MTU |
                                    IBV_QP_DEST_QPN |
                                    IBV_QP_RQ_PSN |
                                    IBV_QP_MAX_DEST_RD_ATOMIC |
                                    IBV_QP_MIN_RNR_TIMER |
                                    IBV_QP_AV)) {
        perror("ibv_modify_qp INIT to RTR");
        return_code = -1;
        goto destroy_qp;
    }

    // RTR to RTS
    qp_attr.qp_state = IBV_QPS_RTS;
    qp_attr.timeout = 14;
    qp_attr.retry_cnt = 7;
    qp_attr.rnr_retry = 7;
    qp_attr.sq_psn = mine.psn;
    qp_attr.max_rd_atomic = 1;
    if (ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE |
                                    IBV_QP_TIMEOUT |
                                    IBV_QP_RETRY_CNT |
                                    IBV_QP_RNR_RETRY |
                                    IBV_QP_SQ_PSN |
                                    IBV_QP_MAX_QP_RD_ATOMIC)) {
        perror("ibv_modify_qp RTR to RTS");
        return_code = -1;
        goto destroy_qp;
    }

    // connection established
    if (is_server) {
        // server post receive
        struct ibv_sge recv_wr_sge;
        memset(&recv_wr_sge, 0, sizeof(recv_wr_sge));
        memset(mr->addr, 0, 1024);
        recv_wr_sge.addr = (uint64_t)mr->addr;
        recv_wr_sge.length = 128;
        recv_wr_sge.lkey = mr->lkey;

        struct ibv_recv_wr recv_wr;
        memset(&recv_wr, 0, sizeof(recv_wr));
        recv_wr.wr_id = 0;
        recv_wr.sg_list = &recv_wr_sge;
        recv_wr.num_sge = 1;

        struct ibv_recv_wr* bad_recv_wr;
        if (ibv_post_recv(qp, &recv_wr, &bad_recv_wr)) {
            if (bad_recv_wr == &recv_wr)
                fprintf(stderr, "ibv_post_recv get bad recv wr");
            perror("ibv_post_recv");
            return_code = -1;
            goto destroy_qp;
        }

        // poll cqe
        struct ibv_wc wc;
        int polled_num;
        do {
            polled_num = ibv_poll_cq(cq, 1, &wc);
        } while (polled_num == 0);
        if (polled_num < 0) {
            perror("ibv_poll_cq");
            return_code = -1;
            goto destroy_cq;
        }
        assert(polled_num == 1);
        printf("polled\n");
        print_wc(&wc);
        printf("%s\n", mr->addr);
    } else {
        // client post send
        char* data = "Hello, world!";
        memcpy(mr->addr, data, strlen(data) + 1);
        struct ibv_sge send_sge;
        send_sge.addr = (uint64_t)mr->addr;
        send_sge.length = strlen(data) + 1;
        send_sge.lkey = mr->lkey;

        struct ibv_send_wr send_wr;
        memset(&send_wr, 0, sizeof(send_wr));
        send_wr.opcode = IBV_WR_SEND;
        send_wr.wr_id = 0;
        send_wr.num_sge = 1;
        send_wr.sg_list = &send_sge;

        struct ibv_send_wr* bad_send_wr;
        if (ibv_post_send(qp, &send_wr, &bad_send_wr)) {
            if (bad_send_wr == &send_wr)
                fprintf(stderr, "ibv_post_send get bad send wr");
            perror("ibv_post_send");
            return_code = -1;
            goto destroy_qp;
        }
        if (bad_send_wr == &send_wr) {
            fprintf(stderr, "ibv_post_send get bad send wr");
            return_code = -1;
            goto destroy_qp;
        }

        // poll cqe
        struct ibv_wc wc;
        int polled_num = 0;
        do {
            polled_num = ibv_poll_cq(cq, 1, &wc);
        } while (polled_num == 0);
        if (polled_num < 0) {
            perror("ibv_poll_cq");
            return_code = -1;
            goto destroy_cq;
        }
        assert(polled_num == 1);
        printf("polled\n");
        print_wc(&wc);
    }

    // clean up
destroy_qp:
    ibv_destroy_qp(qp);
dereg_mr:
    ibv_dereg_mr(mr);
free_mr:
    free(mr_addr);
destroy_cq:
    ibv_destroy_cq(cq);
dealloc_pd:
    ibv_dealloc_pd(pd);
close_device:
    ibv_close_device(context);
free_device_list:
    ibv_free_device_list(device_list);

    return return_code;
}
