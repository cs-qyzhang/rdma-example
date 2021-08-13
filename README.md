# rdma-example

RDMA programming examples using Soft-RoCE.

## Prerequest

rdma-core: <https://github.com/linux-rdma/rdma-core>

## Build

```sh
$ mkdir build && cd build
$ cmake ..
$ make
```

## RoCE Setup

```sh
$ ./setup_roce.sh
```

## Examples

`rc_send_recv.c`:

    RC QP, Send/Receive, Poll CQE, Pure libibverbs
