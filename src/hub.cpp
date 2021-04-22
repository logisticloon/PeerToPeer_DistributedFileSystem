
#include <iostream>
#include <string>
#include <vector>
#include <map>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "hub/hub.h"
#include "hub/dnode_req_server.h"
#include "hub/hub_req_server.h"

#include "util/net.h"
#include "util/hash.h"
#include "util/fs.h"


using namespace std;

// filehash, list_of_dnode_uids mapping
map<uint64_t, vector<uint64_t>> fhash_map;

// to store file_meta_details (file_hash, file_metdata)
map<uint64_t, struct file_metadata_struct *> file_metadata_map; 

// to store file_index_data (file_hash, file_indexdata)
map<uint64_t, uint8_t *> file_indexdata_map;

// filename, filehash mapping
map<string, uint64_t> fname_map;

// to store dnode_details
map<uint64_t, struct dnode_struct *> dnodes_map;


// peer hub details
map<uint64_t, struct peer_hub_struct *> peer_hubs_map;

struct hub_details_struct hub_details;


void initialize_hub() {

    // check for hub root directory
    if (!directory_exists(hub_details.hub_root_dir)) {
        make_dir(hub_details.hub_root_dir);
    }

    // check for chunk_hashes directory
    string chunk_hashes_dir = string(hub_details.hub_root_dir) + string("/chunks");
    memcpy(hub_details.chunk_hashes_dir, chunk_hashes_dir.c_str(), chunk_hashes_dir.size()+1);

    if (!directory_exists(hub_details.chunk_hashes_dir)) {
        make_dir(hub_details.chunk_hashes_dir);
    }

    // check for uid file
    string uid_file_path = string(hub_details.hub_root_dir) + string("/uid_file");
    memcpy(hub_details.uid_file_path, uid_file_path.c_str(), uid_file_path.size() + 1);

    if (access(uid_file_path.c_str(), F_OK) == 0) {
        uint64_t uid;
        int uid_fd = open(uid_file_path.c_str(), O_RDONLY);	
        read(uid_fd, &uid, sizeof(uid));	
        hub_details.uid = uid; //restore the id	
        close(uid_fd);	

    } else {
        uint64_t uid = random64bit();
        hub_details.uid = uid;
        int uid_fd = open(hub_details.uid_file_path, O_CREAT | O_WRONLY, 0644);	
		fwrite_full(uid_fd, &hub_details.uid, sizeof(hub_details.uid));    	
		close(uid_fd);	
    }

    // there are still things to do
}


void hub_hello(char *peer_hub_ip_str) {

    // struct in_addr peer_hub_ip = parse_ip_addr(peer_hub_ip_str);
    // short peer_hub_port = parse_port(peer_hub)
}


/*
./hub root_dir hub_cmd_port dnode_cmd_port selfIP
*/

int main(int argc, char** argv) {

    if (argc < 5) {
        std::cout << "Insufficient arguments" << std::endl;
        std::exit(0);
    }

    char* hub_root_dir = argv[1];
    hub_details.hub_cmd_port = std::stoi(argv[2]);
    hub_details.dnode_cmd_port = std::stoi(argv[3]);
    memcpy(hub_details.hub_root_dir, hub_root_dir, strlen(hub_root_dir) + 1);

    printf("hub root directory : %s\n", hub_details.hub_root_dir);
    printf("hub_cmd_port (listen for peer hubs) : %d\n", hub_details.hub_cmd_port);
    printf("hub_dnode_port (listen for dnodes) : %d\n", hub_details.dnode_cmd_port);

    // if there is 5th argument, i.e. peer hub ip_port
    // if (argc > 4) {
    //     hub_hello(argv[4]);
    // }

    // struct in_addr hub_ip;
    // get_ip_address(&hub_ip);

    std::cout << "self ip : " << argv[4] << std::endl;
    // string self_ip("127.0.0.1");
    string self_ip(argv[4]);
    memcpy(hub_details.hub_ip, self_ip.c_str(), self_ip.size() + 1);

    pthread_t hub_req_server_tid;
    if (pthread_create(&hub_req_server_tid, NULL, handle_hub_req_server, NULL) != 0) {
        perror("Failed to create hub_req_server thread");
        exit(EXIT_FAILURE);
    }

    pthread_t dnode_req_server_tid;
    if (pthread_create(&dnode_req_server_tid, NULL, handle_dnode_req_server, NULL) != 0) {
        perror("Failed to create dnode_req_server thread");
        exit(EXIT_FAILURE);
    }

    pthread_join(hub_req_server_tid, NULL);
    pthread_join(dnode_req_server_tid, NULL);
    
    return 0;
}