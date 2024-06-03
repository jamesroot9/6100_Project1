#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cachesim.hpp"

static void print_help(void);
static int validate_config(sim_config_t *config);
static void print_sim_config(sim_config_t *sim_config);
static void print_legal_sim_config(sim_config_t *sim_config);
static void print_statistics(sim_stats_t* stats, sim_config_t *sim_config);

int main(int argc, char **argv) {
    sim_config_t config = DEFAULT_SIM_CONFIG;
    int opt;

    /* Read arguments */
    while(-1 != (opt = getopt(argc, argv, "c:b:s:p:t:m:vh"))) {
        switch(opt) {
        case 'c': // c
            config.c = atoi(optarg);
            break;
        case 'b': // b
            config.b = atoi(optarg);
            break;
        case 's':
            config.s = atoi(optarg);
            break;
        case 'v': // vipt
            config.vipt = true;
            break;
        case 'p': // log2 page size
            config.p = atoi(optarg);
            break;
        case 't': // log2 tlb entries
            config.t = atoi(optarg);
            break;
        case 'm': // log2 num pages in phys mem
            config.m = atoi(optarg);
            break;
        case 'h':
            /* Fall through */
        default:
            print_help();
            return 0;
        }
    }

    if (config.vipt) printf("Initital ");
    printf("Cache Settings\n");
    printf("--------------\n");
    print_sim_config(&config);
    printf("\n");

    if (config.vipt) {
        legalize_s(&config);
        printf("\nLegalized Cache Settings\n");
        printf("--------------\n");
        print_legal_sim_config(&config);
        printf("\n");
    }

    if (validate_config(&config)) {
        return 1;
    }

    // if (config.vipt) {
    //     legalize_s(&config);
    //     printf("\nLegalized Cache Settings\n");
    //     printf("--------------\n");
    //     print_legal_sim_config(&config);
    //     printf("\n");
    // }

    /* Setup the cache */

    sim_setup(&config);

    /* Setup statistics */
    sim_stats_t stats;
    memset(&stats, 0, sizeof stats);

    /* Begin reading the file */
    char rw;
    uint64_t address;
    while (!feof(stdin)) {
        int ret = fscanf(stdin, "%c 0x%" PRIx64 "\n", &rw, &address);
        if(ret == 2) {
            sim_access(rw, address, &stats);
        }
    }

    sim_finish(&stats);

    print_statistics(&stats, &config);

    return 0;
}

static void print_help(void) {
    printf("cachesim [OPTIONS] < traces/file.trace\n");
    printf("-h\t\tThis helpful output\n");
    printf("L1 parameters:\n");
    printf("  -c C\t\tTotal size for L1 in bytes is 2^C\n");
    printf("  -b B\t\tSize of each block for L1 in bytes is 2^B\n");
    printf("  -s S\t\tNumber of blocks (ways) per set for L1 is 2^S\n");
    printf("Virtual Memory Parameters:\n");
    printf("  -v \t\tEnable Virtual Memory\n");
    printf("  -p P\t\tTotal size in bytes for a page is 2^P\n");
    printf("  -t T\t\tNumber of entries in the TLB is 2^T\n");
    printf("  -m M\t\tNumber of Pages in Memory is 2^M (same as entries in HWIVPT)\n");
    printf("  -D   \t\tDisable L2 cache\n");
}

static int validate_config(sim_config_t *config) {
    if (config->b > 7 || config->b < 4) {
        printf("Invalid configuration! The block size must be reasonable: 4 <= B <= 7\n");
        return 1;
    }

    if (config->c > 18 || config->c < 9) {
        printf("Invalid configuration! The cache size must be reasonable: 9 <= C <= 18\n");
        return 1;
    }

    if (config->vipt) {
        if (config->p < 9 || config->p > 14 || config->p > config->c) {
            printf("Invalid configuration! The page size must be reasonable: 9 <= P <= min(14, C)\n");
            return 1;
        }

        if (config->t > (config->c - config->b - config->s)) {
            printf("Invalid configuration! The TLB must have a reasonable number of entries: 0 <= T <= C - B - S\n");
            return 1;
        }

        if (config->m < config->p || config->m > 20) {
            printf("Invalid configuration! Do not simulate too few/many pages in memory: P <= M <= 20\n");
            return 1;
        }

        if (config->m + config->p > 32) {
            printf("Invalid configuration! Do not simulate too much memory (4GB): 0 <= M <= 32 - P\n");
            return 1;
        }
    }
    

    return 0;
}

static void print_sim_config(sim_config_t *sim_config) {
    if (sim_config->vipt) {
        printf("(C,B,S): (%" PRIu64 ",%" PRIu64 ",x)\n",
        sim_config->c, sim_config->b
        );
        printf("Assume Virtual Addresses\n");
        printf("\tlog2(Page Size): %" PRIu64 "\n", sim_config->p);
        printf("\tlog2(TLB entries): %" PRIu64 "\n", sim_config->t);
        printf("\tlog2(Physical Pages in Memory): %" PRIu64 "\n", sim_config->m);
    } else {
        printf("(C,B,S): (%" PRIu64 ",%" PRIu64 ",%" PRIu64 ")\n",
        sim_config->c, sim_config->b, sim_config->s
        );
        printf("Assume Physical Addresses\n");
    }
    int i = 0;
    i++;
}

static void print_legal_sim_config(sim_config_t *sim_config) {
    if (sim_config->vipt) {
        printf("(C,B,S): (%" PRIu64 ",%" PRIu64 ",%" PRIu64 ")\n",
        sim_config->c, sim_config->b, sim_config->s
        );
        printf("Assume Virtual Addresses\n");
        printf("\tlog2(Page Size): %" PRIu64 "\n", sim_config->p);
        printf("\tlog2(TLB entries): %" PRIu64 "\n", sim_config->t);
        printf("\tlog2(Physical Pages in Memory): %" PRIu64 "\n", sim_config->m);
    } else {
        printf("(C,B,S): (%" PRIu64 ",%" PRIu64 ",%" PRIu64 ")\n",
        sim_config->c, sim_config->b, sim_config->s
        );
        printf("Assume Physical Addresses\n");
    }
    int i = 0;
    i++;
}

static void print_statistics(sim_stats_t* stats, sim_config_t *config) {
    printf("Cache Statistics\n");
    printf("----------------\n");
    printf("Reads: %" PRIu64 "\n", stats->reads);
    printf("Writes: %" PRIu64 "\n", stats->writes);
    printf("\n");
    if (config->vipt) {
        printf("HWIVPT accesses: %" PRIu64 "\n", stats->accesses_hw_ivpt);
        printf("HWIVPT hits: %" PRIu64 "\n", stats->hits_hw_ivpt);
        printf("HWIVPT misses (Page Faults): %" PRIu64 "\n", stats->misses_hw_ivpt);
        printf("HWIVPT hit ratio: %.3f\n", stats->hit_ratio_hw_ivpt);
        printf("HWIVPT miss ratio: %.3f\n", stats->miss_ratio_hw_ivpt);
        printf("L1 writebacks due to OS cache flush: %" PRIu64 "\n", stats->cache_flush_writebacks);
        printf("\n");

        printf("TLB accesses: %" PRIu64 "\n", stats->accesses_tlb);
        printf("TLB hits: %" PRIu64 "\n", stats->hits_tlb);
        printf("TLB misses (Translation Faults): %" PRIu64 "\n", stats->misses_tlb);
        printf("TLB hit ratio: %.3f\n", stats->hit_ratio_tlb);
        printf("TLB miss ratio: %.3f\n", stats->miss_ratio_tlb);
        printf("\n");
    }
    printf("L1 accesses: %" PRIu64 "\n", stats->accesses_l1);
    printf("L1 hits: %" PRIu64 "\n", stats->hits_l1);
    printf("L1 misses: %" PRIu64 "\n", stats->misses_l1);
    printf("L1 hit ratio: %.3f\n", stats->hit_ratio_l1);
    printf("L1 miss ratio: %.3f\n", stats->miss_ratio_l1);
    printf("L1 writebacks due user level conflicts: %" PRIu64 "\n", stats->writebacks_l1);
    printf("L1 average access time (AAT): %.3f\n", stats->avg_access_time);
    printf("\n");
}
