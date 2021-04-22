
#include <time.h>

#include "hash.h"

/*
To do, we should be able to compute hash on data with any size, not just a multiple
of 64 bits (need to padd the data)
*/

// computes 64 bit hash
uint64_t compute_hash(const void * data, size_t l)
{
  uint32_t i;
  struct fhash hstrct = {0};
  uint64_t * d = (uint64_t *)data;
  const uint64_t fnv_prime = 1099511628211ull;
  hstrct.a = 14695981039346656037ull;
  hstrct.b = 14695981739346656037ull;
  hstrct.c = 14695981039346756037ull;
  hstrct.d = 14795981039346656037ull;
  for(i = 0; i < l / 8; i++)
  {
    hstrct.a = (hstrct.a ^ d[i]) * fnv_prime;
    hstrct.b = (hstrct.b ^ d[i]) * fnv_prime;
    hstrct.c = (hstrct.c ^ d[i]) * fnv_prime;
    hstrct.d = (hstrct.d ^ d[i]) * fnv_prime;
  }

  // return hstrct;
  return (hstrct.a) ^ (hstrct.b) ^ (hstrct.c) ^ (hstrct.d);
}

uint64_t random64bit() {

  srand(time(NULL));
  uint64_t lhalf = (uint64_t)rand() << 32;
  uint64_t rhalf = (uint64_t)(rand());

  return lhalf ^ rhalf;
}

uint64_t reduce_hash(struct fhash * h1) {
    return (h1->a) ^ (h1->b) ^ (h1->c) ^ (h1->d);
}


int compare_hashes(struct fhash *h1, struct fhash *h2) {
    return (h1->a == h2->a) && (h1->b && h2->b) && (h1->c && h2->c) && (h1->d && h1->d); 
}