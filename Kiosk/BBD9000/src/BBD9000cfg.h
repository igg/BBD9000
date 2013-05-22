#ifndef BBD9000cfg_h
#define BBD9000cfg_h

#include <confuse.h> // for reading configuration file
#include "BBD9000mem.h"

cfg_t *conf_cfg_init (BBD9000mem *shmem);
int conf_cfg_read (BBD9000mem *shmem);

cfg_t *cal_cfg_init (BBD9000mem *shmem);
int cal_cfg_read (BBD9000mem *shmem);
void cal_cfg_from_mem (cfg_t *cfg,BBD9000mem *shmem);
void cal_cfg_to_mem (cfg_t *cfg,BBD9000mem *shmem);
void cal_cfg_write (BBD9000mem *shmem);

cfg_t *run_cfg_init (BBD9000mem *shmem);
int run_cfg_read  (BBD9000mem *shmem);
void run_cfg_write (BBD9000mem *shmem);

void print_float2(cfg_opt_t *opt, unsigned int index, FILE *fp);
void print_float3(cfg_opt_t *opt, unsigned int index, FILE *fp);

int netlink (BBD9000mem *shmem, const char *msg, char get_lock);
int netlock (BBD9000mem *shmem);
void netunlock (BBD9000mem *shmem, int fdlock);

#endif /* BBD9000cfg_h */
