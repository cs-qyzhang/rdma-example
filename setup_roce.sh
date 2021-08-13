#!/bin/bash

# active roce module
sudo modprobe rdma_rxe
# create virtual ethernet interface
sudo ip link add veth0 type veth peer name veth1
# configure veth and bring them up
sudo ifconfig veth0 192.168.2.2 netmask 255.255.255.0 up
sudo ifconfig veth1 192.168.2.3 netmask 255.255.255.0 up
# create roce device over veth
sudo rdma link add rxe_veth0 type rxe netdev veth0
sudo rdma link add rxe_veth1 type rxe netdev veth1
