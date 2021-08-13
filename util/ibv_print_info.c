#include <stdio.h>
#include <assert.h>
#include "util/ibv_print_info.h"

const char* transport_type_str(enum ibv_transport_type type) {
    switch (type) {
        case IBV_TRANSPORT_IB:          return "IBV_TRANSPORT_IB";
        case IBV_TRANSPORT_IWARP:       return "IBV_TRANSPORT_IWARP";
        case IBV_TRANSPORT_UNSPECIFIED: return "IBV_TRANSPORT_UNSPECIFIED";
        case IBV_TRANSPORT_USNIC_UDP:   return "IBV_TRANSPORT_USNIC_UDP";
        case IBV_TRANSPORT_USNIC:       return "IBV_TRANSPORT_USNIC";
        case IBV_TRANSPORT_UNKNOWN:     return "IBV_TRANSPORT_UNKNOWN";
        default:                        return "IBV_TRANSPORT_???";
    }
}

const char* atomic_cap_str(enum ibv_atomic_cap cap) {
    switch (cap) {
        case IBV_ATOMIC_NONE: return "IBV_ATOMIC_NONE";
        case IBV_ATOMIC_HCA:  return "IBV_ATOMIC_HCA";
        case IBV_ATOMIC_GLOB: return "IBV_ATOMIC_GLOB";
        default:              return "IBV_ATOMIC_???";
    }
}

const char* max_mtu_str(enum ibv_mtu mtu) {
    switch (mtu) {
        case IBV_MTU_256:  return "IBV_MTU_256";
        case IBV_MTU_512:  return "IBV_MTU_512";
        case IBV_MTU_1024: return "IBV_MTU_1024";
        case IBV_MTU_2048: return "IBV_MTU_2048";
        case IBV_MTU_4096: return "IBV_MTU_4096";
        default:           return "IBV_MTU_???";
    }
}

const char* qp_type_str(enum ibv_qp_type type) {
    switch (type) {
        case IBV_QPT_RC:            return "IBV_QPT_RC";
        case IBV_QPT_UC:            return "IBV_QPT_UC";
        case IBV_QPT_UD:            return "IBV_QPT_UD";
        case IBV_QPT_RAW_PACKET:    return "IBV_QPT_RAW_PACKET";
        case IBV_QPT_DRIVER:        return "IBV_QPT_DRIVER";
        case IBV_QPT_XRC_RECV:      return "IBV_QPT_XRC_RECV";
        case IBV_QPT_XRC_SEND:      return "IBV_QPT_XRC_SEND";
        default:                    return "IBV_QPT_???";
    }
}

const char* wc_opcode_str(enum ibv_wc_opcode op) {
    switch (op) {
        case IBV_WC_SEND:               return "IBV_WC_SEND";
	    case IBV_WC_RDMA_WRITE:         return "IBV_WC_RDMA_WRITE";
	    case IBV_WC_RDMA_READ:          return "IBV_WC_RDMA_READ";
	    case IBV_WC_COMP_SWAP:          return "IBV_WC_COMP_SWAP";
	    case IBV_WC_FETCH_ADD:          return "IBV_WC_FETCH_ADD";
	    case IBV_WC_BIND_MW:            return "IBV_WC_BIND_MW";
	    case IBV_WC_LOCAL_INV:          return "IBV_WC_LOCAL_INV";
	    case IBV_WC_TSO:                return "IBV_WC_TSO";
	    case IBV_WC_RECV:               return "IBV_WC_RECV";
	    case IBV_WC_RECV_RDMA_WITH_IMM: return "IBV_WC_RECV_RDMA_WITH_IMM";
	    case IBV_WC_TM_ADD:             return "IBV_WC_TM_ADD";
	    case IBV_WC_TM_DEL:             return "IBV_WC_TM_DEL";
	    case IBV_WC_TM_SYNC:            return "IBV_WC_TM_SYNC";
	    case IBV_WC_TM_RECV:            return "IBV_WC_TM_RECV";
	    case IBV_WC_TM_NO_TAG:          return "IBV_WC_TM_NO_TAG";
	    case IBV_WC_DRIVER1:            return "IBV_WC_DRIVER1";
	    default:                        return "IBV_WC_???";
    }
}

void print_device_cap_flags(unsigned int flags) {
    unsigned int caps[] = {
        IBV_DEVICE_RESIZE_MAX_WR,
        IBV_DEVICE_BAD_PKEY_CNTR,
        IBV_DEVICE_BAD_QKEY_CNTR,
        IBV_DEVICE_RAW_MULTI,
        IBV_DEVICE_AUTO_PATH_MIG,
        IBV_DEVICE_CHANGE_PHY_PORT,
        IBV_DEVICE_UD_AV_PORT_ENFORCE,
        IBV_DEVICE_CURR_QP_STATE_MOD,
        IBV_DEVICE_SHUTDOWN_PORT,
        IBV_DEVICE_INIT_TYPE,
        IBV_DEVICE_PORT_ACTIVE_EVENT,
        IBV_DEVICE_SYS_IMAGE_GUID,
        IBV_DEVICE_RC_RNR_NAK_GEN,
        IBV_DEVICE_SRQ_RESIZE,
        IBV_DEVICE_N_NOTIFY_CQ,
        IBV_DEVICE_XRC,
    };
    const char* caps_str[] = {
        "IBV_DEVICE_RESIZE_MAX_WR",
        "IBV_DEVICE_BAD_PKEY_CNTR",
        "IBV_DEVICE_BAD_QKEY_CNTR",
        "IBV_DEVICE_RAW_MULTI",
        "IBV_DEVICE_AUTO_PATH_MIG",
        "IBV_DEVICE_CHANGE_PHY_PORT",
        "IBV_DEVICE_UD_AV_PORT_ENFORCE",
        "IBV_DEVICE_CURR_QP_STATE_MOD",
        "IBV_DEVICE_SHUTDOWN_PORT",
        "IBV_DEVICE_INIT_TYPE",
        "IBV_DEVICE_PORT_ACTIVE_EVENT",
        "IBV_DEVICE_SYS_IMAGE_GUID",
        "IBV_DEVICE_RC_RNR_NAK_GEN",
        "IBV_DEVICE_SRQ_RESIZE",
        "IBV_DEVICE_N_NOTIFY_CQ",
        "IBV_DEVICE_XRC",
    };

    printf("device_cap_flags: %#X\n", flags);
    for (int i = 0; i < sizeof(caps) / sizeof(caps[0]); ++i)
        if (flags & caps[i])
            printf("  %s\n", caps_str[i]);
}

void print_device_list(struct ibv_device** device_list, int num_dev) {
    assert(device_list != NULL && num_dev >= 1);
    printf("-------------------- device list --------------------\n");
    printf("num_dev: %d\n", num_dev);

    for (int i = 0; i < num_dev; ++i) {
        struct ibv_device* dev = device_list[i];
        printf("  dev %d:\n", i);
        printf("    dev_name: %s\n", dev->dev_name);
        printf("    name: %s\n", dev->name);
        printf("    dev_path: %s\n", dev->dev_path);
        printf("    ibdev_path: %s\n", dev->ibdev_path);
        printf("    ibv_node_type: %s\n", ibv_node_type_str(dev->node_type));
        printf("    transport_type: %s\n", transport_type_str(dev->transport_type));
    }
}

void print_device_attr(struct ibv_device_attr* attr, int index) {
    printf("-------------------- device %d --------------------\n", index);
    printf("fw_ver: %s\n", attr->fw_ver);
    printf("phys_port_cnt: %d\n", attr->phys_port_cnt);
    printf("atomic_cap: %s\n", atomic_cap_str(attr->atomic_cap));
    print_device_cap_flags(attr->device_cap_flags);
    printf("node_guid: %#llx\n", attr->node_guid);
    printf("local_ca_ack_delay: %d\n", attr->local_ca_ack_delay);
}

void print_port_attr(struct ibv_port_attr* attr, int port_num) {
    printf("-------------------- port %d --------------------\n", port_num);
    printf("state: %s\n", ibv_port_state_str(attr->state));
    printf("max_mtu: %s\n", max_mtu_str(attr->max_mtu));
    printf("active_mtu: %s\n", max_mtu_str(attr->active_mtu));
    printf("gid_tbl_len: %d\n", attr->gid_tbl_len);
    printf("max_msg_sz: %u\n", attr->max_msg_sz);
    printf("lid: %hu\n", attr->lid);
    printf("sm_lid: %hu\n", attr->sm_lid);
    printf("sm_sl: %hhu\n", attr->sm_sl);
    printf("max_vl_num: %hhu\n", attr->max_vl_num);
    printf("active_width: %hhu\n", attr->active_width);
    printf("active_speed: %hhu\n", attr->active_speed);
    printf("phys_state: %hhu\n", attr->phys_state);
}

void print_gid(union ibv_gid* gid, int index) {
    printf("-------------------- gid --------------------\n");
    printf("gid %d:\n");
    printf("  subnet_prefix: %#llx, interface_id: %#llx\n",
            gid->global.subnet_prefix, gid->global.interface_id);
}

void print_pkey(uint16_t pkey, int index) {
    printf("-------------------- pkey --------------------\n");
    printf("pkey %d: %hu\n", index, pkey);
}

void print_qp_init_attr(struct ibv_qp_init_attr* attr) {
    printf("-------------------- qp init attr --------------------\n");
    printf("qp_type: %s\n", qp_type_str(attr->qp_type));
    printf("srq: %p\n", attr->srq);
    printf("cap.max_inline_data: %u\n", attr->cap.max_inline_data);
    printf("cap.max_recv_sge: %u\n", attr->cap.max_recv_sge);
    printf("cap.max_recv_wr: %u\n", attr->cap.max_recv_wr);
    printf("cap.max_send_sge: %u\n", attr->cap.max_send_sge);
    printf("cap.max_send_wr: %u\n", attr->cap.max_send_wr);
}

void print_wc(struct ibv_wc* wc) {
    printf("-------------------- wc --------------------\n");
    printf("status: %s\n", ibv_wc_status_str(wc->status));
    printf("opcode: %s\n", wc_opcode_str(wc->opcode));
    printf("byte_len: %u\n", wc->byte_len);
    if (wc->wc_flags & IBV_WC_WITH_IMM)
        printf("immediate: %u\n", wc->imm_data);
}