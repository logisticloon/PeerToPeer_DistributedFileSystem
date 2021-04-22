
#ifndef _HASH_H_
#define _HASH_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct fhash {
    uint64_t a;
    uint64_t b;
    uint64_t c;
    uint64_t d;
};

uint64_t compute_hash(const void * data, size_t l);

uint64_t random64bit();

uint64_t reduce_hash(struct fhash *h1);

int compare_hashes(struct fhash *hash1, struct fhash *hash2);



#endif