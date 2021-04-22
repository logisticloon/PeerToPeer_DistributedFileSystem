
#ifndef _HUB_H_
#define _HUB_H_

#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../util/net.h"

#define FILE_CHUNK_SIZE (1024*1024)  // 1 mb
#define FILE_CHUNK_HASH_SIZE 8 // 64 bits

// commands used by a dnode
#define NODE_JOIN            0x00000001
#define NODE_HELLO           0x00000002
#define FILE_UPLOAD          0x00000003
#define FILE_DOWNLOAD        0x00000004
#define FILE_DOWNLOADED_ACK  0x00000005

// commands used by peer hub
#define HUB_JOIN             0x00010001
#define HUB_HELLO            0x00010002
#define HUB_QUERY_FILE_SRCS  0x00010003


struct hub_cmd_struct {
    uint32_t cmd_type;
};

inline void send_cmd_to_hub(int hub_sockfd, uint16_t command) {
    struct hub_cmd_struct hub_cmd;
    hub_cmd.cmd_type = command;
    send_full(hub_sockfd, &hub_cmd, sizeof(hub_cmd));
}

// hub details

struct hub_details_struct {
    uint64_t uid;
    // struct in_addr hub_ip;
    char hub_ip[16];
    int hub_cmd_port;
    int dnode_cmd_port;
    char hub_root_dir[512];
    char chunk_hashes_dir[512];
    char uid_file_path[512];
};

// hub only data structures

// store details about a peer hub
struct peer_hub_struct {
    uint64_t uid;
    // struct in_addr ip;
    char ip[16];
    short port;
    uint16_t flags;
};

struct dnode_struct{ // details about a dnode
    uint64_t uid;
    // struct in_addr ip;
    char ip[16];
    short port;
    uint16_t flags;
};

struct file_metadata_struct { // details about a file
    uint64_t file_hash;
    int file_size;
    int num_chunks;
    int file_index_data_size;
};

// public data structures (hub and peer dnodes)

struct peer_dnode_struct {
    // struct in_addr ip;
    char ip[16];
    short port;
    short flags;
};

// NEW_NODE_JOIN

struct node_join_req_struct {
    // struct in_addr ip;
    char ip[16];
    short port;
    uint16_t flags;
};

struct node_join_res_struct {
    uint64_t uid;
};

// NODE HELLO
struct node_hello_req_struct {
    uint64_t dnode_id;
    // struct in_addr ip;
    char ip[16];
    short port;
    uint16_t flags;
};

struct node_hello_res_struct {
    uint64_t uid;
};

// FILE UPLOAD 

struct file_upload_req_struct {
    uint64_t dnode_uid;
    uint64_t file_hash;
    int file_size;
    char file_name[64]; // assuming all the files of size less than this.
    int num_chunks;
    int file_index_data_size;
};

struct file_upload_res_struct {

};

// FILE DOWNLOAD

struct file_download_req_struct {
    uint64_t dnode_uid;
    char file_name[64];
};

struct file_download_res_struct {
    uint64_t file_hash;
    int file_size;
    int num_chunks;
    int num_peer_dnodes;
    int file_index_data_size;
};

// FILE DOWNLOADED ACK 

struct file_downloaded_ack_struct {
    uint64_t dnode_uid;
    uint64_t file_hash;
};


/*===============================*/
/*== HUB TO HUB COMMUNICATION ===*/
/*===============================*/

struct hub_join_req_struct {
    uint64_t uid;
    // struct in_addr ip;
    char ip[16];
    short port;
    int flags;
};

struct hub_join_res_struct {
    uint64_t dummy;
};


struct query_file_srcs_req_struct {
    uint64_t file_hash;
};

struct query_file_srcs_res_struct {
    uint64_t num_dnodes;
};

struct src_dnode_struct {
    uint64_t dnode_id;
    // struct in_addr ip;
    char ip[16];
    short port;
    int flags;
};


#endif