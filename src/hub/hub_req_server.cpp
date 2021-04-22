
#include <iostream>
#include <string>
#include <vector>
#include <map>

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>

#include "hub.h"
#include "hub_req_server.h"
#include "../util/net.h"
#include "../util/hash.h"


using namespace std;

// filehash, list_of_dnode_uids mapping
extern map<uint64_t, vector<uint64_t>> fhash_map;

// to store file_meta_details (file_hash, file_metdata)
extern map<uint64_t, struct file_metadata_struct *> file_metadata_map; 

// to store file_index_data (file_hash, file_indexdata)
extern map<uint64_t, uint8_t *> file_indexdata_map;

// filename, filehash mapping
extern map<string, uint64_t> fname_map;

// to store dnode_details
extern map<uint64_t, struct dnode_struct *> dnodes_map;

extern map<uint64_t, struct peer_hub_struct *> peer_hubs_map;


// hub list
extern hub_details_struct hub_details;

void handle_hub_join(int peer_hubfd) {
    
    // get hub join request
    struct hub_join_req_struct hub_join_req;
    recv_full(peer_hubfd, &hub_join_req, sizeof(hub_join_req));

    printf("hub join : uid %08lx\n", hub_join_req.uid);

    // fill up peer_hub_struct
    uint64_t peer_hub_id = hub_join_req.uid;
    struct peer_hub_struct *peer_hub_details;
    if (peer_hubs_map.find(peer_hub_id) != peer_hubs_map.end()) {
        peer_hub_details = peer_hubs_map[peer_hub_id];
    } else {
        peer_hub_details = (struct peer_hub_struct *)malloc(sizeof(struct peer_hub_struct));
    }

    peer_hub_details->uid = hub_join_req.uid;
    // peer_hub_details->ip = hub_join_req.ip;
    memcpy(peer_hub_details->ip, hub_join_req.ip, strlen(hub_join_req.ip) + 1);
    peer_hub_details->port = hub_join_req.port;

    // save peer hub details
    peer_hubs_map[peer_hub_id] = peer_hub_details;

    return;
}

void handle_hub_hello(int peer_hubfd) {

}

void handle_query_for_file_srcs(int peer_hubfd) {

    // get query for file sources request
    struct query_file_srcs_req_struct file_src_req;
    recv_full(peer_hubfd, &file_src_req, sizeof(file_src_req));

    uint64_t file_hash = file_src_req.file_hash;

    // lookup to get file sources
    vector<uint64_t> dnode_ids = fhash_map[file_hash];
    int num_src_nodes = dnode_ids.size();

    // initialize peer_dnodes_list
    struct src_dnode_struct *src_dnodes_list;
    int src_dnodes_list_size = sizeof(struct peer_dnode_struct) * num_src_nodes;
    src_dnodes_list = (struct src_dnode_struct *)malloc(src_dnodes_list_size);

    for(int i=0; i < dnode_ids.size(); i++) {
        struct dnode_struct *dnode = dnodes_map[dnode_ids[i]];
        src_dnodes_list[i].dnode_id = dnode->uid;
        // src_dnodes_list[i].ip = dnode->ip;
        memcpy(src_dnodes_list[i].ip, dnode->ip, strlen(dnode->ip) + 1);
        src_dnodes_list[i].port = dnode->port;
        src_dnodes_list[i].flags = 0;
    }

    // send response
    struct query_file_srcs_res_struct file_srcs_res;
    file_srcs_res.num_dnodes = num_src_nodes;
    send_full(peer_hubfd, &file_srcs_res, sizeof(file_srcs_res));

    // send src_dnodes list
    send_full(peer_hubfd, &src_dnodes_list, src_dnodes_list_size);
}


void *handle_hub_req_server(void *args) {

    int sockfd = create_server(hub_details.hub_cmd_port);
    
    if (listen(sockfd, 5) == 0) {
        printf("Hub :: listening for hub requests on %d\n", hub_details.hub_cmd_port);
    }

    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);

    while(1) {
        
        int peer_hubfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (peer_hubfd < 0) {
            perror("Accept error\n");
            std::exit(0);
        }

        printf("\nHub_cmd_server :: Handling new request\n");

        struct hub_cmd_struct hub_cmd;
        recv_full(peer_hubfd, &hub_cmd, sizeof(hub_cmd));

        if (hub_cmd.cmd_type == HUB_JOIN) {
            printf("Hub_cmd_server :: Hub join request\n");
            handle_hub_join(peer_hubfd);
        
        } 
        else if (hub_cmd.cmd_type == HUB_HELLO) {
            printf("Hub_cmd_server :: Hub hello request\n");
            handle_hub_hello(peer_hubfd);

        } 
        else if (hub_cmd.cmd_type == HUB_QUERY_FILE_SRCS) {
            printf("Hub_cmd_server :: Query for file sources request");
            handle_query_for_file_srcs(peer_hubfd);
            
        }
        else {
            printf("Invalid command\n");
        }

        close(peer_hubfd);
    }

    close(sockfd);
} 