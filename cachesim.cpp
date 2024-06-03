#include "cachesim.hpp"
#include <cstddef>

struct tag {
    uint64_t tag;
    uint8_t dirty;
    uint8_t valid;
    uint64_t access;
};

struct way {
    struct tag* tags;
};

struct lru_way {
    uint64_t* lru_tag;
};

struct page {
    uint64_t PPN;
    uint64_t VPN;
    uint8_t valid;
    uint64_t access;
};

struct way* g_cache;
uint64_t g_num_ways;
uint64_t g_tags_per_way;

uint64_t g_tag_mask;
uint64_t g_index_mask;

uint64_t g_tag_offset;
uint64_t g_index_offset;
uint64_t g_s;
uint64_t g_m;

uint64_t g_tlb_entries;
uint64_t g_hwivpt_entries;
uint64_t g_hwivpt_valids;

uint64_t g_next_ppn;

struct page* g_tlb;
struct page* g_hwivpt;



struct lru_way* g_lru_ways;

/**
 * The use of virtually indexed physically tagged caches limits 
 *      the total number of sets you can have.
 * If the user selected configuration is invalid for VIPT
 *      Update config->s to reflect the minimum value for S (log2 number of ways)
 * If the user selected configuration is valid
 *      do not modify it.
 * TODO: You're responsible for completing this routine
*/

void legalize_s(sim_config_t *config) {
    //if invalid, fix
    if (config->vipt)
        // check if minimum s is not met
        if (config->s < config->c - config->p) {
            // update s to minimum value
            config->s = config->c - config->p;
            #ifdef DEBUG
            printf("Use of VIPT cache required reassignment of S\n");
            #endif
        }
        // don't update if above minimum
    
}

/**
 * Subroutine for initializing the cache simulator. You many add and initialize any global or heap
 * variables as needed.
 * TODO: You're responsible for completing this routine
 */

void sim_setup(sim_config_t *config) {
    //establish spacing for tag store, size 2^(c-b) * (PA(=p+m) - (c-s) + 2)

    //extract c, b, s for reading ease.
    uint64_t c = config->c;
    uint64_t b = config->b;
    uint64_t s = config->s;
    g_s = s;
    g_m = config->m;

    //establish global indexing values
    g_num_ways = 1 << config->s;
    g_tags_per_way = 1 << (config->c - config->b - config->s);

    // establish the number of ways
    g_cache = new struct way[g_num_ways];
    g_lru_ways = new struct lru_way[g_num_ways];

    //for each way, define the list of tag blocks AND the tridiagonal matricies
    for (uint64_t i = 0; i < g_num_ways; i++)
    {
        g_cache[i].tags = new struct tag[g_tags_per_way];
        g_lru_ways[i].lru_tag = new uint64_t[g_tags_per_way];
        // establish all valid bits to be 0.
        for (uint64_t j = 0; j < g_tags_per_way; j++)
        {
            g_cache[i].tags[j].valid = 0;
            g_cache[i].tags[j].access = 0;
            g_lru_ways[i].lru_tag[j] = g_tags_per_way - i;
        }

    }

    //define tag and index masks
    uint64_t PA;//Physical address size = P + M
    //if (config->vipt) {
    //    PA = config->p + config->m;
    //} else {
    PA = 63;
    //}
    g_tag_offset = c-s;
    g_tag_mask = 1;
    g_tag_mask = (((g_tag_mask << PA) - 1) >> (c-s)) << (c-s);
    g_index_offset = b;
    g_index_mask = (((1 << (c-s)) - 1) >> (b)) << (b); //may need to add one to c - b - s

    
    //printf("PA Size: %ld\nTag Offset: %ld \nTag Mask: %ld \nIndex Offset: %ld \nIndex Mask: %ld\n\n", PA, g_tag_offset, g_tag_mask, g_index_offset, g_index_mask);

    //define TLB and HWIVPT
    if (config->vipt) {
        g_tlb_entries = 1 << config->t;
        g_tlb = new struct page[g_tlb_entries];
        for (uint64_t i = 0; i < g_tlb_entries; i++) {
            //establish each page translation
            g_tlb[i].access = 0;
            g_tlb[i].valid = 0;
            g_tlb[i].PPN = 0;
            g_tlb[i].VPN = 0;
        }

        g_hwivpt_entries = 1 << config->m;
        g_hwivpt_valids = 0;
        g_next_ppn = (1 << config->m) - 1;
        g_hwivpt = new page[g_hwivpt_entries];
        for (uint64_t i = 0; i < g_hwivpt_entries; i++) {
            //establish each page translation
            g_hwivpt[i].access = 0;
            g_hwivpt[i].valid = 0;
            g_hwivpt[i].PPN = i;
            g_hwivpt[i].VPN = 0;
        }
    }

    

}

/**
 * Subroutine that simulates the cache one trace event at a time.
 * TODO: You're responsible for completing this routine
 */
void sim_access(char rw, uint64_t addr, sim_stats_t* stats) {
    #ifdef DEBUG
    //printf("Use of VIPT cache required reassignment of S\n");
    printf("Time: %ld. Address: 0x%lx. Read/Write: %c\n", stats->accesses_l1, addr, rw);
    #endif

    //  L1 cache is accessed. update stat.
    stats->accesses_l1++;
    stats->accesses_tlb++;

    // Determine tag and index bits (block offset bits not necessary for our implementation)
    uint64_t addr_tag = (addr & g_tag_mask) >> g_tag_offset;
    uint64_t addr_idx = (addr & g_index_mask) >> g_index_offset;
    #ifdef DEBUG
    uint64_t addr_vpn = addr_tag;
    #endif
    #ifdef DEBUG
    printf("\tArray Access at index: 0x%lx\n", addr_idx);
    #endif

    // check if item is in cache AND valid (hit)
    uint64_t lru_idx = 0;
    // iterate through each way of the determined set and compare
    stats->array_lookups_l1++;
    bool found;
    // array has been accessed (we know it is at addr_idx), here we perform the TLB check
    if (g_tlb)
    {
        found = false;
        // check for VPN match
        for (uint64_t i = 0; i < g_tlb_entries; i++)
        {
            // check for match
            if (g_tlb[i].valid && g_tlb[i].VPN == addr_tag)
            {
                // TLB HIT
                #ifdef DEBUG
                printf("\tFound TLB Entry for VPN: 0x%" PRIx64 " with PPN: 0x%" PRIx64 "\n", g_tlb[i].VPN, g_tlb[i].PPN);
#endif
                stats->hits_tlb++;
                addr_tag = g_tlb[i].PPN;
                g_tlb[i].access = stats->accesses_tlb;
                g_hwivpt[g_tlb[i].PPN].access = stats->accesses_tlb;
                found = true;
                break;
            }
        }
        // check for TLB miss, access the HWIVPT if we missed
        if (!found)
        {
            #ifdef DEBUG
            printf("\tNo valid TLB Entry for VPN: 0x%" PRIx64 ", Translation Fault\n", addr_tag);
            #endif
            stats->misses_tlb++;
            stats->accesses_hw_ivpt++;
            // we must scan the HWIVPT to see if the valid VPN-PPN pair exists
            for (uint64_t i = 0; i < g_hwivpt_entries; i++)
            {
                // check for match
                if (g_hwivpt[i].valid && g_hwivpt[i].VPN == addr_tag)
                {
                    // hit

                    #ifdef DEBUG
                    printf("\tFound HWIVPT entry for mapping with VPN: 0x%" PRIx64 ", PFN: 0x%" PRIx64 "\n", addr_tag, g_hwivpt[i].PPN);
                    #endif

                    found = true;
                    stats->hits_hw_ivpt++;
                    addr_tag = g_hwivpt[i].PPN;
                    // repair the tlb (find LRU and swap with i)
                    uint64_t lru_idx = 0;
                    for (uint64_t j = 0; j < g_tlb_entries; j++)
                    {
                        if (g_tlb[j].access < g_tlb[lru_idx].access)
                            lru_idx = j;
                    }
                    // place info into TLB at LRU, make MRU.
                    g_tlb[lru_idx].access = stats->accesses_tlb;
                    g_tlb[lru_idx].valid = 1;
                    g_tlb[lru_idx].PPN = g_hwivpt[i].PPN;
                    g_tlb[lru_idx].VPN = g_hwivpt[i].VPN;

                    g_hwivpt[i].access = stats->accesses_tlb;
                    // match is already stored in HWIVPT so no need to place in.
                    break;
                }
            }
        }
        // check for HWIVPT miss, access Dynamic Paging Daemon if needed.
        if (!found)
        {

            stats->misses_hw_ivpt++;

            // we need to check if a nonexistent mapping in hwivpt is not utilized
            uint64_t lru_hwivpt = 0;
            if (g_hwivpt_valids < g_hwivpt_entries) {
                //next available pfn mapping is available at .._valids
                lru_hwivpt = g_hwivpt_entries - g_hwivpt_valids - 1;
                g_hwivpt_valids++;
            } else {
                // we will have to put an item at the lru mapping in the HWIVPT
                for (uint64_t i = 0; i < g_hwivpt_entries; i++) {
                    if (g_hwivpt[i].access < g_hwivpt[lru_hwivpt].access) {
                        lru_hwivpt = i;
                    }
                }
            }

            #ifdef DEBUG
            printf("\tNo valid HWIVPT entry for VPN: 0x%" PRIx64 ", Page Fault\n", addr_tag);
            printf("\tFlushing cache to emulate cache pollution from OS handler\n");
            printf("\tInstalling HWIVPT entry for mapping with VPN: 0x%" PRIx64 ", PFN: 0x%" PRIx64 "\n", addr_tag, lru_hwivpt);
            printf("\tInstalling TLB entry for mapping with VPN: 0x%" PRIx64 ", PFN: 0x%" PRIx64 "\n", addr_tag, lru_hwivpt);
            #endif

            // place mapping into the tlb at LRU position
            // find lru index of tlb
            uint64_t lru_idx = 0;
            for (uint64_t i = 0; i < g_tlb_entries; i++)
            {
                if (g_tlb[i].access < g_tlb[lru_idx].access)
                {
                    lru_idx = i;
                }
            }
            g_hwivpt[lru_hwivpt].valid = 1;
            g_hwivpt[lru_hwivpt].access = stats->accesses_tlb;
            g_hwivpt[lru_hwivpt].VPN = addr_tag;

            g_tlb[lru_idx].PPN = lru_hwivpt;
            g_tlb[lru_idx].valid = 1;
            g_tlb[lru_idx].access = stats->accesses_tlb;
            g_tlb[lru_idx].VPN = addr_tag;

            // flush!
            for (uint64_t w = 0; w < g_num_ways; w++)
            {
                for (uint64_t x = 0; x < g_tags_per_way; x++)
                {
                    // perform any write backs needed before wipe
                    if (g_cache[w].tags[x].dirty)
                    {
                        stats->cache_flush_writebacks++;
                    }
                }
                g_cache[w].tags = new struct tag[g_tags_per_way];
                g_lru_ways[w].lru_tag = new uint64_t[g_tags_per_way];
                // establish all valid bits to be 0.
                for (uint64_t x = 0; x < g_tags_per_way; x++)
                {
                    // clear bits
                    g_cache[w].tags[x].valid = 0;
                    g_cache[w].tags[x].access = 0;
                    g_cache[w].tags[x].dirty = 0;
                    g_cache[w].tags[x].tag = 0;
                    g_lru_ways[w].lru_tag[x] = g_tags_per_way - w;
                }
            }

            // item goes straight to memory, not the cache
            // stats->misses_l1++;

            addr_tag = lru_hwivpt;
            
            found = true;
            //return;
        }
    } else {
        //for debug print usage
        found = true;
    }



    // Determine if instruction is reading or writing
    if (rw == READ) {
        // case of read from cache
        stats->reads++;

        lru_idx = 0;

        for (uint64_t i = 0; i < g_num_ways; i++) {
            stats->tag_compares_l1++;
            #ifdef DEBUG
            //printf("\t\tComparison between %ld and %ld, access: %ld\n", g_cache[i].tags[addr_idx].tag, addr_tag, g_cache[i].tags[addr_idx].access);
            #endif
            if (g_cache[i].tags[addr_idx].valid == 1 && g_cache[i].tags[addr_idx].tag == addr_tag) {
                // we have a hit, update statistics
                #ifdef DEBUG
                printf("\tTag match in index: 0x%" PRIx64 "Block: valid = %d, tag = %" PRIx64 "\n", addr_idx, g_cache[i].tags[addr_idx].valid, g_cache[i].tags[addr_idx].tag);
                #endif

                stats->hits_l1++;
                // update tag block info
                g_cache[i].tags[addr_idx].valid = 1;
                g_cache[i].tags[addr_idx].access = stats->accesses_l1;
                // our work for this instruction is done, return from access
                return;
            }
            // this item is not found in this way, see if the idx in this way is LRU than previous ones and update accordingly.
            if (g_cache[i].tags[addr_idx].access < g_cache[lru_idx].tags[addr_idx].access) {
                lru_idx = i;
            }
        }
        
        //printf("CACHE MISS\n");
        //no match found, read data from memory and place in lru block
        stats->misses_l1++;
        #ifdef DEBUG
        if (found) {
            printf("\tNo tag match found in index: 0x%" PRIx64 ", tag = 0x%" PRIx64 "\n", addr_idx, addr_tag);
            printf("\tEvicting Block with Tag: 0x%" PRIx64 ", Dirty: %d\n", g_cache[lru_idx].tags[addr_idx].tag, g_cache[lru_idx].tags[addr_idx].dirty);
        }
        printf("\tInstalling Block with Tag: 0x%" PRIx64 ", Dirty: %d\n", addr_tag, g_cache[lru_idx].tags[addr_idx].dirty);
        printf("\tUpdating HWIVPT LRU status for mapping with VPN: 0x%" PRIx64 ", PFN: 0x%" PRIx64 "\n", addr_vpn, addr_tag); 
        #endif

        // check for writeback, and act if needed
        if (g_cache[lru_idx].tags[addr_idx].valid && g_cache[lru_idx].tags[addr_idx].dirty)
            stats->writebacks_l1++;
        // block can now be cleaned/updated
        g_cache[lru_idx].tags[addr_idx].dirty = 0;
        g_cache[lru_idx].tags[addr_idx].tag = addr_tag;
        g_cache[lru_idx].tags[addr_idx].valid = 1;
        g_cache[lru_idx].tags[addr_idx].access = stats->accesses_l1;

        //if using VIPT, update this evicted block to be LRU
        if (!found) {
            g_hwivpt[addr_tag].access = 0;
            for (uint64_t j = 0; j < g_tlb_entries; j++){
                if (g_tlb[j].PPN == addr_tag)
                    g_tlb[j].access = 0;
            }
        }
        
        
    } 
    else if (rw == WRITE) {
        // case of write to cache
        stats->writes++;

        // iterate through each way and compare (sequential search)
        for (uint64_t i = 0; i < g_num_ways; i++) {
            stats->tag_compares_l1++;
            #ifdef DEBUG
            //printf("\t\tComparison between %ld and %ld, access: %ld\n", g_cache[i].tags[addr_idx].tag, addr_tag, g_cache[i].tags[addr_idx].access);
            #endif
            if (g_cache[i].tags[addr_idx].valid == 1 && g_cache[i].tags[addr_idx].tag == addr_tag) {
                // we have a hit, update statistics
                stats->hits_l1++;

                #ifdef DEBUG
                printf("\tTag match in index: 0x%" PRIx64 "Block: valid = %d, tag = %" PRIx64 "\n", addr_idx, g_cache[i].tags[addr_idx].valid, g_cache[i].tags[addr_idx].tag);
                #endif

                //update tag block info for WB
                g_cache[i].tags[addr_idx].valid = 1;
                g_cache[i].tags[addr_idx].dirty = 1;
                g_cache[i].tags[addr_idx].access = stats->accesses_l1;

                // our work for this instruction is done, return from access
                //printf("CACHE HIT\n");
                return;
            }
            // this item is not found in this way, see if the idx in this way is LRU than previous ones and update accordingly.
            if (g_cache[i].tags[addr_idx].access < g_cache[lru_idx].tags[addr_idx].access)
            {
                lru_idx = i;
            }
        }

        //no match found, replace info in lru block with new data
        stats->misses_l1++;

        #ifdef DEBUG
        if (found)
        {
            printf("\tNo tag match found in index: 0x%" PRIx64 ", tag = 0x%" PRIx64 "\n", addr_idx, addr_tag);
            printf("\tEvicting Block with Tag: 0x%" PRIx64 ", Dirty: %d\n", g_cache[lru_idx].tags[addr_idx].tag, g_cache[lru_idx].tags[addr_idx].dirty);
        }
        printf("\tInstalling Block with Tag: 0x%" PRIx64 ", Dirty: %d\n", addr_tag, g_cache[lru_idx].tags[addr_idx].dirty);
        printf("\tUpdating HWIVPT LRU status for mapping with VPN: 0x%" PRIx64 ", PFN: 0x%" PRIx64 "\n", addr_vpn, addr_tag);
        #endif

        // check for writeback, and act if needed
        if (g_cache[lru_idx].tags[addr_idx].valid && g_cache[lru_idx].tags[addr_idx].dirty)
            stats->writebacks_l1++;
        // block can now be cleaned/updated
        g_cache[lru_idx].tags[addr_idx].dirty = 1;
        g_cache[lru_idx].tags[addr_idx].tag = addr_tag;
        g_cache[lru_idx].tags[addr_idx].valid = 1;
        g_cache[lru_idx].tags[addr_idx].access = stats->accesses_l1;

        // item is now in cache
        // if using VIPT, update this evicted block to be LRU
        if (!found)
        {
            g_hwivpt[addr_tag].access = 0;
            for (uint64_t j = 0; j < g_tlb_entries; j++)
            {
                    if (g_tlb[j].PPN == addr_tag)
                        g_tlb[j].access = 0;
            }
        }
    }


}


/**
 * Subroutine for cleaning up any outstanding memory operations and calculating overall statistics
 * such as miss rate or average access time.
 * TODO: You're responsible for completing this routine
 */
void sim_finish(sim_stats_t *stats) {
    //calculate final statistics
    stats->hit_ratio_l1 = (1.0 * stats->hits_l1) / stats->accesses_l1;
    stats->miss_ratio_l1 = (1.0 * stats->misses_l1) / stats->accesses_l1;
    double HT = L1_ARRAY_LOOKUP_TIME_CONST + L1_TAG_COMPARE_TIME_CONST + g_s * L1_TAG_COMPARE_TIME_PER_S;
    stats->avg_access_time = HT + stats->miss_ratio_l1 * 100;

    stats->hit_ratio_tlb = (1.0 * stats->hits_tlb) / stats->accesses_tlb;
    stats->miss_ratio_tlb = (1.0 * stats->misses_tlb) / stats->accesses_tlb;

    stats->hit_ratio_hw_ivpt = (1.0 * stats->hits_hw_ivpt) / stats->accesses_hw_ivpt;
    stats->miss_ratio_hw_ivpt = (1.0 * stats->misses_hw_ivpt) / stats->accesses_hw_ivpt;

    if (g_next_ppn) {
        
        double hw_iv_pen = (1 + HW_IVPT_ACCESS_TIME_PER_M * g_m) * DRAM_ACCESS_PENALTY;
        double tag_compare = L1_TAG_COMPARE_TIME_PER_S * g_s + L1_TAG_COMPARE_TIME_CONST;
        HT = L1_ARRAY_LOOKUP_TIME_CONST + stats->hit_ratio_tlb * tag_compare + stats->miss_ratio_tlb * (hw_iv_pen + tag_compare * stats->hit_ratio_hw_ivpt);
        stats->avg_access_time = HT + stats->miss_ratio_l1 * 100;
    }
}
