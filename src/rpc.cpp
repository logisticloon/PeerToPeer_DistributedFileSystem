#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <iostream>
#include <string>

#include "dnode/dnode.h"
#include "util/net.h"

using namespace std;

/*
cmd :: ./rfc dnodeip it is sending request to:port -u file_path
cmd :: ./rfc dnodeip it is sending request to:port -d file_name
*/

int main(int argc, char* argv[]) {

    if (argc < 4) {
        printf("insufficient arguments..\n");
        exit(0);
    }

    std::cout << "dnode ip:port ::" << argv[1] << std::endl;
    std::cout << "command :: " << argv[2] << std::endl;
    std::cout << "file path/name :: " << argv[3] << std::endl;

    char dnode_ip_addr[16];
    parse_ip_addr(argv[1], dnode_ip_addr);
    short dnode_port = parse_port(argv[1]);

    int dnode_sockfd = connect_to_server(dnode_ip_addr, dnode_port);
    std::cout << "RPC :: connected to dnode.." << std::endl;

    char buffer[1024];
    struct rpc_req_struct rpc_req;
    char* payload = argv[3];
    int payload_len = strlen(payload) + 1; //to include null byte

    if (string(argv[2]) == "-u") {
        rpc_req.req_type = RPC_REQ_UPLOAD;
        rpc_req.payload_len = payload_len;

    } else if (string(argv[2]) == "-d") {
        rpc_req.req_type = RPC_REQ_DOWNLOAD;
        rpc_req.payload_len = payload_len;

    } else {
        printf("Invalid command, %s\n", argv[2]);
        exit(0);
    }


    std::cout << "RPC :: sending rpc request " << std::endl;
    send_full(dnode_sockfd, &rpc_req, sizeof(rpc_req));

    std::cout << "RPC :: sending payload " << std::endl;
    send_full(dnode_sockfd, payload, payload_len);

    // read response from the dnode server
    struct rpc_res_struct rpc_res;
    recv_full(dnode_sockfd, &rpc_res, sizeof(rpc_res));

    if (rpc_res.res_type == RPC_RES_SUCCEES) {
        std::cout << "RPC :: op success !!" << std::endl;

    } else if (rpc_res.res_type == RPC_RES_FAILURE) {
        std::cout << "RPC :: op failure !!" << std::endl;
    
    } else {
        std::cout << "Invalid response type" << std::endl;
    }

    //close connection
    disconnect_from_server(dnode_sockfd);

    return 0;
}