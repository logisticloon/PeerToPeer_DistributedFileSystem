
/*
cmd : ./dnode hub_port dnode_port rfc_port
listens for commands or messages from an hub via hub_port
listens for data transfer requests from other data nodes via dnode_port
listens for rfc commands via rfc_port
**/
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include <string>

#include "dnode/dnode.h"
#include "dnode/rpc_server.h"
#include "dnode/data_server.h"

#include "hub/hub.h"
#include "util/net.h"
#include "util/fs.h"

using namespace std;


struct dnode_details_struct dnode_details;


void initialize_dnode(string &dnode_root_dir) {

    // create dnode root directory if it doesn't exist
    if (!directory_exists(dnode_root_dir.c_str())) {
        make_dir(dnode_root_dir.c_str());
    }

    string dnode_files_dir = dnode_root_dir + string("/files");
    string dnode_meta_dir = dnode_root_dir + string("/meta");

    if (!directory_exists(dnode_files_dir.c_str())) {
        make_dir(dnode_files_dir.c_str());
    }

    if (!directory_exists(dnode_meta_dir.c_str())) {
        make_dir(dnode_meta_dir.c_str());
    }

    std::cout << "dnode_root_dir  :: " << dnode_root_dir << std::endl;
    std::cout << "dnode_files_dir :: " << dnode_files_dir << std::endl;
    std::cout << "dnode_meta_dir  :: " << dnode_meta_dir << std::endl;

    // store folders
    memcpy(dnode_details.root_dir, dnode_root_dir.c_str(), dnode_root_dir.size()+1);
    memcpy(dnode_details.files_dir, dnode_files_dir.c_str(), dnode_files_dir.size() + 1);
    memcpy(dnode_details.meta_dir, dnode_meta_dir.c_str(), dnode_meta_dir.size() + 1);

    string uid_file_path = dnode_meta_dir + string("/uid_file");
    memcpy(dnode_details.uid_file_path, uid_file_path.c_str(), uid_file_path.size() + 1);

    if( access(uid_file_path.c_str(), F_OK ) == 0 ) { // file exists
        uint64_t uid;
        int uid_fd = open(uid_file_path.c_str(), O_RDONLY);
        read(uid_fd, &uid, sizeof(uid));
        dnode_details.uid = uid; //restore the id
        close(uid_fd);
        
    } else {
        // file doesn't exist
        dnode_details.uid = 0;
    }

    return;
}

void handle_node_join() {

    // connect to hub
    int hub_sockfd = connect_to_server(dnode_details.hub_ip, dnode_details.hub_port);

    // send hub command
    struct hub_cmd_struct hub_cmd;
    hub_cmd.cmd_type = NODE_JOIN;
    send_full(hub_sockfd, &hub_cmd, sizeof(hub_cmd));

    // send node join request
    struct node_join_req_struct node_join_req;
    memcpy(node_join_req.ip, dnode_details.dnode_ip, sizeof(dnode_details.dnode_ip) + 1);
    // node_join_req.ip = dnode_details.dnode_ip;
    node_join_req.port = dnode_details.dnode_data_port;
    node_join_req.flags = 0;
    send_full(hub_sockfd, &node_join_req, sizeof(node_join_req));

    // recv node join response
    struct node_join_res_struct node_join_res;
    recv_full(hub_sockfd, &node_join_res, sizeof(node_join_res));

    // save uid given by hub (in fs as well)
    dnode_details.uid = node_join_res.uid;
    int uid_fd = open(dnode_details.uid_file_path, O_CREAT | O_WRONLY, 0644);
    fwrite_full(uid_fd, &dnode_details.uid, sizeof(dnode_details.uid));    
    close(uid_fd);

    close(hub_sockfd);
    return;
}

void handle_node_hello() {

    // connect to hub
    int hub_sockfd = connect_to_server(dnode_details.hub_ip, dnode_details.hub_port);

    // send hub command
    struct hub_cmd_struct hub_cmd;
    hub_cmd.cmd_type = NODE_HELLO;
    send_full(hub_sockfd, &hub_cmd, sizeof(hub_cmd));

    // send node hello request
    struct node_hello_req_struct node_hello_req;
    node_hello_req.dnode_id = dnode_details.uid;
    // node_hello_req.ip = dnode_details.dnode_ip;
    memcpy(node_hello_req.ip, dnode_details.dnode_ip, sizeof(dnode_details.dnode_ip) + 1);
    node_hello_req.port = dnode_details.dnode_data_port;
    node_hello_req.flags = 0;
    send_full(hub_sockfd, &node_hello_req, sizeof(node_hello_req));

    // recv node hello response
    struct node_hello_res_struct node_hello_res;
    recv_full(hub_sockfd, &node_hello_res, sizeof(node_hello_res));

    close(hub_sockfd);
    return;
}

/*
cmd : ./dnode dnode_dir hub_ip:port hub_port dnode_port rfc_port selfIP
*/

int main(int argc, char* argv[]) {

    if (argc < 7) {
        std::cout << "not enough arguments!!" << endl;
        std::exit(1);
    }

    // std::cout << "dnode dir :: " << argv[1] << std::endl;
    // std::cout << "hub ip:port :: " << argv[2] << std::endl;
    // std::cout << "hub cmd port :: " << argv[3] << std::endl;
    // std::cout << "dnode data port :: " << argv[4] << std::endl;
    // std::cout << "rpc cmd port :: " << argv[5] << std::endl; 

    // store hub details
    // dnode_details.hub_ip = parse_ip_addr(argv[2]);
    parse_ip_addr(argv[2], dnode_details.hub_ip);
    dnode_details.hub_port = parse_port(argv[2]);

    std::cout << "parsed_hub_ip : " << dnode_details.hub_ip << std::endl;

    /*todo : should fill this properly*/
    // inet_aton("127.0.0.1", (dnode_details.dnode_ip));
    
    // store port details
    dnode_details.hub_cmd_port = (short)atoi(argv[3]);
    dnode_details.dnode_data_port = (short)atoi(argv[4]);
    dnode_details.rpc_port = (short)atoi(argv[5]);

    std::cout << "self ip : " << argv[6] << std::endl;
    // string self_ip("127.0.0.1");
    string self_ip(argv[6]);
    memcpy(dnode_details.dnode_ip, self_ip.c_str(), self_ip.size() + 1);

    // initialize dnode
    string dnode_root_dir = string(argv[1]);
    initialize_dnode(dnode_root_dir);
    printf("dnode uid : %08lx\n", dnode_details.uid);

    // in case the node is joining for first time
    if(dnode_details.uid) { 
        handle_node_hello();
    } else {
        handle_node_join();
    }
    printf("dnode uid : %08lx\n", dnode_details.uid); 
    
    // char dnode_buf[2048];

    // if (fork() == 0) {
    //     handle_hub_server(hub_port);
    // }

    pthread_t rpc_server_tid;
    if (pthread_create(&rpc_server_tid, NULL, handle_rpc_server, NULL) != 0) {
        perror("Failed to create rpc server thread");
        exit(EXIT_FAILURE);
    }

    sleep(1);

    pthread_t data_server_tid;
    if (pthread_create(&data_server_tid, NULL, handle_data_server, NULL) != 0) {
        perror("Failed to create data server thread\n");
        exit(EXIT_FAILURE);
    }

    pthread_join(rpc_server_tid, NULL);
    pthread_join(data_server_tid, NULL);

    std::exit(0);
}