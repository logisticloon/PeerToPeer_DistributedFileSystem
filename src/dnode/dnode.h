
#ifndef _DNODE_H_
#define _DNODE_H_

#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define RPC_REQ_UPLOAD 0x0001
#define RPC_REQ_DOWNLOAD 0x0002
#define RPC_REQ_KILL 0x0003

#define RPC_RES_SUCCEES 0x0101
#define RPC_RES_FAILURE 0x0102

#define DATA_TRANSFER_SUCCESS 0x00010001
#define DATA_TRANSFER_FAILURE 0X00010001

#define PEER_IS_OFFLINE 0x00000000
#define PEER_IS_ONLINE  0x00000001

#define DOWNLOADABLE_CHUNKS 10 //one time downloadable chunks
#define DOWNLOAD_THREADS 4

// RPC rerquest and response

struct rpc_req_struct {
    uint16_t req_type;
    short payload_len;
};

struct rpc_res_struct {
    uint16_t res_type;
    short payload_len;
};

// Data server, File request
struct file_data_req_struct {
    uint64_t file_hash;
    char file_name[64];
    int offset;
    int size;
};

struct file_data_res_struct {
    uint64_t res_type;
    int payload_len;
};

struct dnode_details_struct {
    // struct in_addr hub_ip;
    char hub_ip[16];
    short hub_port;
    // struct in_addr dnode_ip;
    char dnode_ip[16];
    short hub_cmd_port;
    short dnode_data_port;
    short rpc_port;
    uint64_t uid;
    char root_dir[256];
    char files_dir[256];
    char meta_dir[256];
    char uid_file_path[256];
};

struct dt_thread_args_struct {
    int sockfd;
};


struct download_src_dnode_struct {
    // struct in_addr ip;
    char ip[16];
    short port;
    int is_online;
    int chunks_served;
};

struct download_chunk_struct {
    int index;
    uint64_t chunk_hash;
    int offset;
    int size;
    int is_downloading;
    int is_downloaded;
};

struct download_threads_args_struct {
    uint64_t file_hash;
    char file_name[128];
    uint8_t *download_buffer;
    int current_chunk_index;
    int max_chunk_index;
    int num_peer_nodes;
    int num_chunks;
    int *num_chunks_downloaded_ptr;
    int *num_bytes_downloaded_ptr;
    struct download_src_dnode_struct *src_dnodes_list;
    struct download_chunk_struct *chunks_list;
    pthread_mutex_t *num_chunks_downloaded_lock;
    pthread_mutex_t *src_dnodes_list_lock;
    pthread_mutex_t *chunks_list_lock;
};

#endif