/*
 * print information of some ibverbs structures
 */
#pragma once

#include <infiniband/verbs.h>

// enum to string
const char* transport_type_str(enum ibv_transport_type type);
const char* atomic_cap_str(enum ibv_atomic_cap cap);
const char* max_mtu_str(enum ibv_mtu mtu);
const char* qp_type_str(enum ibv_qp_type type);
const char* wc_opcode_str(enum ibv_wc_opcode op);

void print_device_cap_flags(unsigned int flags);
void print_device_list(struct ibv_device** device_list, int num_dev);
void print_device_attr(struct ibv_device_attr* attr, int index);
void print_port_attr(struct ibv_port_attr* attr, int port_num);
void print_gid(union ibv_gid* gid, int index);
void print_pkey(uint16_t pkey, int index);
void print_qp_init_attr(struct ibv_qp_init_attr* attr);
void print_wc(struct ibv_wc* wc);