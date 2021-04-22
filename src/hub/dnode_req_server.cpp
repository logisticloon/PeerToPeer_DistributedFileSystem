
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

#include "hub.h"
#include "dnode_req_server.h"
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


extern hub_details_struct hub_details;

void handle_new_node_join(int dnode_sockfd) {

    // get node join request from dnode
    struct node_join_req_struct node_join_req;
    recv_full(dnode_sockfd, &node_join_req, sizeof(node_join_req));

    // generate random id for the dnode
    uint64_t dnode_id = random64bit();
    printf("new node joined : uid %08lx\n", dnode_id);

    // fill up dnode details
    struct dnode_struct *dnode_details = (struct dnode_struct *)malloc(sizeof(struct dnode_struct));
    dnode_details->uid = dnode_id;
    // dnode_details->ip = node_join_req.ip;
    memcpy(dnode_details->ip, node_join_req.ip, strlen(node_join_req.ip) + 1);
    dnode_details->port = node_join_req.port;
    dnode_details->flags = 0; /*TODO :*/

    // save dnode in memory
    dnodes_map[dnode_id] = dnode_details;

    // send result back to dnode
    struct node_join_res_struct node_join_res;
    node_join_res.uid = dnode_id;
    send(dnode_sockfd, &node_join_res, sizeof(node_join_res), 0);

    return;
}


void handle_node_hello(int dnode_sockfd) {

    // get node join request from dnode
    struct node_hello_req_struct node_hello_req;
    recv_full(dnode_sockfd, &node_hello_req, sizeof(node_hello_req));

    // generate random id for the dnode
    printf("node hello : uid %08lx\n", node_hello_req.dnode_id);

    // fill up dnode details
    uint64_t dnode_id = node_hello_req.dnode_id;
    struct dnode_struct *dnode_details;
    if (dnodes_map.find(dnode_id) != dnodes_map.end()) {
        dnode_details = dnodes_map[dnode_id];
    } else {
        dnode_details = (struct dnode_struct *)malloc(sizeof(struct dnode_struct));
    }
     
    dnode_details->uid = node_hello_req.dnode_id;
    // dnode_details->ip = node_hello_req.ip;
    memcpy(dnode_details->ip, node_hello_req.ip, strlen(node_hello_req.ip) + 1);
    dnode_details->port = node_hello_req.port;
    dnode_details->flags = 0; /*TODO :*/

    // save dnode in memory
    dnodes_map[dnode_id] = dnode_details;

    // send result back to dnode
    struct node_hello_res_struct node_hello_res;
    node_hello_res.uid = dnode_id;
    send(dnode_sockfd, &node_hello_res, sizeof(node_hello_res), 0);

    return;
}


void handle_file_upload_req(int dnode_sockfd) {
    
    // char file_index_data[4096]; // 4MB will suffice for this project

    // get file upload request
    struct file_upload_req_struct file_upload_req;
    recv_full(dnode_sockfd, &file_upload_req, sizeof(file_upload_req));

    // parse file upload request
    uint64_t dnode_id  = file_upload_req.dnode_uid;
    uint64_t file_hash = file_upload_req.file_hash;
    int file_size = file_upload_req.file_size;
    string file_name = string(file_upload_req.file_name);
    int num_chunks = file_upload_req.num_chunks;
    int file_index_data_size = file_upload_req.file_index_data_size;
    
    // store the file details in file_metadata_map
    struct file_metadata_struct *metadata = (file_metadata_struct *)malloc(sizeof(file_metadata_struct));
    metadata->file_hash = file_hash;
    metadata->file_size = file_size;
    metadata->num_chunks = num_chunks;
    metadata->file_index_data_size = file_index_data_size;

    file_metadata_map[file_hash] = metadata;

    // if the file_hash entry is absent in fhashmap
    if (fhash_map.find(file_hash) == fhash_map.end()) {
        fhash_map[file_hash] = vector<uint64_t>();
    }
    fhash_map[file_hash].push_back(dnode_id);
            
    // add the fname entry
    if (fname_map.find(file_name) == fname_map.end()) {
        fname_map[file_name] = file_hash;
    }

    // recv file_index_data;
    uint8_t *file_index_data = (uint8_t *)malloc(file_index_data_size);
    recv_full(dnode_sockfd, file_index_data, file_index_data_size);
    file_indexdata_map[file_hash] = file_index_data;

    /*TODO: should send something back to dnode*/

    return;
}


void handle_file_download_req(int dnode_sockfd) {

    // get file download request
    struct file_download_req_struct file_download_req;
    recv_full(dnode_sockfd, &file_download_req, sizeof(file_download_req));

    // parse file download request
    uint64_t dnode_uid = file_download_req.dnode_uid;
    string file_name =  string(file_download_req.file_name);
    uint64_t fhash =  fname_map[file_name];

    // intialize file_index_data // todo

    // fetch dnodes which contain the file with fhash
    vector<uint64_t> dnode_ids = fhash_map[fhash];
    int num_peer_nodes = dnode_ids.size();

    // collect peer file sources from other hubs
    // int num_peer_src_nodes = 0;
    // if (peer_hubs_map.size() > 0) {
    //     // for now, only from one peer hub
    //     auto it = peer_hubs_map.begin();
    //     struct peer_hub_struct * 
        
    // }

    // initialize peer_dnodes_list
    struct peer_dnode_struct * peer_dnodes_list;
    int peer_dnodes_list_size = sizeof(struct peer_dnode_struct) * num_peer_nodes;
    peer_dnodes_list = (struct peer_dnode_struct *)malloc(peer_dnodes_list_size);

    for(int i=0; i < dnode_ids.size(); i++) {
        struct dnode_struct *dnode = dnodes_map[dnode_ids[i]];
        // peer_dnodes_list[i].ip = dnode->ip;
        memcpy(peer_dnodes_list[i].ip, dnode->ip, strlen(dnode->ip) + 1);
        peer_dnodes_list[i].port = dnode->port;
        peer_dnodes_list[i].flags = 0;
    }

    
    // send download response
    struct file_download_res_struct file_download_res;
    file_download_res.file_hash = fhash;
    file_download_res.file_size = file_metadata_map[fhash]->file_size;
    file_download_res.num_chunks = file_metadata_map[fhash]->num_chunks;   
    file_download_res.num_peer_dnodes = num_peer_nodes;
    file_download_res.file_index_data_size = file_metadata_map[fhash]->file_index_data_size;
    send_full(dnode_sockfd, &file_download_res, sizeof(file_download_res));

    //send file index data
    int file_index_data_size = file_download_res.file_index_data_size;
    uint8_t *file_index_data = file_indexdata_map[fhash];
    send_full(dnode_sockfd, file_index_data, file_index_data_size);
    
    //send peer nodes data
    send_full(dnode_sockfd, peer_dnodes_list, peer_dnodes_list_size);

    return;
}


void handle_file_downloaded_ack(int dnode_sockfd) {

    // get file_downloaded_ack
    struct file_downloaded_ack_struct file_downloaded_ack;
    recv_full(dnode_sockfd, &file_downloaded_ack, sizeof(file_downloaded_ack));

    // parse contents
    uint64_t dnode_uid = file_downloaded_ack.dnode_uid;
    uint64_t file_hash =  file_downloaded_ack.file_hash;

    // append the dnode to file sources map
    if (fhash_map.find(file_hash) == fhash_map.end()) {
        fhash_map[file_hash] = vector<uint64_t>();
    }
    fhash_map[file_hash].push_back(dnode_uid);
    
    return;
}


void *handle_dnode_req_server(void *args) {

    

    int sockfd = create_server(hub_details.dnode_cmd_port);

    if (listen(sockfd, 5) == 0) {
        printf("Hub :: listening for dnode requests on %d\n", hub_details.dnode_cmd_port);
    }

    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);

    while(1) {

        int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) {
            perror("Accept error\n");
            std::exit(0);
        }

        std::cout << "\nHub :: Handling request" << std::endl;
        struct hub_cmd_struct hub_cmd;
        recv_full(newsockfd, &hub_cmd, sizeof(hub_cmd));

        if(hub_cmd.cmd_type == NODE_JOIN) {
            std::cout << "Hub :: Node join request" << std::endl;
            handle_new_node_join(newsockfd);

        } 
        else if (hub_cmd.cmd_type == NODE_HELLO) {
            std::cout << "Hub :: Node hello request" << std::endl;
            handle_node_hello(newsockfd);

        }
        else if (hub_cmd.cmd_type == FILE_UPLOAD) {
            std::cout << "Hub :: File upload request" << std::endl;
            handle_file_upload_req(newsockfd);

        }
        else if (hub_cmd.cmd_type == FILE_DOWNLOAD) {
            std::cout << "Hub :: File download request" << std::endl;
            handle_file_download_req(newsockfd);
        }

        else if (hub_cmd.cmd_type == FILE_DOWNLOADED_ACK) {
            std::cout << "Hub :: File downloaded acknowledgement" << std::endl;
            handle_file_downloaded_ack(newsockfd);
        }

        else {
            std::cout << "Invalid request" << hub_cmd.cmd_type << std::endl;
        }
        

        close(newsockfd);

    }

    close(sockfd);
}
