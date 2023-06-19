#include <stdio.h>

#include "client.h"

int main(){
    struct rpc_connection rpc = RPC_init(1246/*1237*/, 63514, "127.0.0.1");

    RPC_idle(&rpc, 5);
    RPC_put(&rpc, 10, 10);

    printf("Put %d\n", 10);

    RPC_close(&rpc);
}
