#ifndef CACHESIM_HPP
#define CACHESIM_HPP

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>

typedef struct sim_config {
    // (C,B,S) in the Conte Cache Taxonomy (Patent Pending)
    uint64_t c;
    uint64_t s; // legalized based on C, B, P
    uint64_t b;
    bool vipt; // whether or not to use VIPT
    uint64_t p; // log2(page size)
    uint64_t t; // log2(number of TLB entries)
    uint64_t m; // log2(number of pages in memory)
} sim_config_t;

typedef struct sim_stats {
    uint64_t reads;                 // read requests
    uint64_t writes;                // write requests
    uint64_t accesses_l1;           // total attempts to use L1
    uint64_t array_lookups_l1;      // times an array lookup was used to populate the set buffer
    uint64_t tag_compares_l1;       // times the L1 was used and TLB hit
    uint64_t hits_l1;               // times (TLB hit and) tag matched
    uint64_t misses_l1;             // times (TLB hit and) tag mismatch
    uint64_t writebacks_l1;         // total L1 evictions of dirty blocks
    double hit_ratio_l1;            // ratio of tag matches to TLB hits for L1 cache
    double miss_ratio_l1;           // ratio of tag mismatches to TLB hits for L1 cache
    uint64_t accesses_tlb;      // times TLB accessed
    uint64_t hits_tlb;          // times TLB hit
    uint64_t misses_tlb;        // times TLB missed (translation fault)
    double hit_ratio_tlb;       // ratio of TLB hits to TLB accesses
    double miss_ratio_tlb;       // ratio of TLB misses to TLB accesses
    uint64_t accesses_hw_ivpt;   // times HW inverted page table accessed
    uint64_t hits_hw_ivpt;       // times there was a hit in HW_IPT after a TLB miss
    uint64_t misses_hw_ivpt;     // times there was a miss in HW_IPT after a TLB miss (page fault)
    double hit_ratio_hw_ivpt;       // ratio of HWIVPT hits to HWIVPT accesses
    double miss_ratio_hw_ivpt;       // ratio of HWIVPT misses to HWIVPT accesses
    uint64_t cache_flush_writebacks;
    double avg_access_time;     // average access time for the entire system
} sim_stats_t;

extern void legalize_s(sim_config_t *config);
extern void sim_setup(sim_config_t *config);
extern void sim_access(char rw, uint64_t addr, sim_stats_t* p_stats);
extern void sim_finish(sim_stats_t *p_stats);

// Sorry about the /* comments */. C++11 cannot handle basic C99 syntax,
// unfortunately
static const sim_config_t DEFAULT_SIM_CONFIG = {
        /*.c =*/ 12, // 4KB Cache
        /*.s =*/ 0, // default direct mapped if possible
        /*.b =*/ 6,  // 64-byte blocks
        /*.vipt=*/ false, // addresses are physical addresses
        /*.p =*/ 10, // 1KB Page size
        /*.t =*/ 3,  // 32 entries in TLB
        /*.m =*/ 10  // 1024 pages fit in physical memory
};

// Argument to cache_access rw. Indicates a load
static const char READ = 'R';
// Argument to cache_access rw. Indicates a store
static const char WRITE = 'W';

static const double DRAM_ACCESS_PENALTY = 100;
// Hit time (HT) for an L1 Cache:
// is HIT_TIME_CONST + (HIT_TIME_PER_S * S)
static const double L1_ARRAY_LOOKUP_TIME_CONST = 1;
static const double L1_TAG_COMPARE_TIME_CONST = 1;
static const double L1_TAG_COMPARE_TIME_PER_S = 0.2;

static const double TLB_HIT_TIME = 1;

// HW_IVPT_PENALTY = (1+HW_IVPT_ACCESS_TIME_PER_M * M) * DRAM_ACCESS_PENALTY
static const double HW_IVPT_ACCESS_TIME_PER_M = 0.01;

#endif /* CACHESIM_HPP */
