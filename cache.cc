/***************************************************************************
*	Name : Sourav Dokania
*	UIN	 : 832001209
*	File : cache.cc
*	Usage: [input_stream_data] | ./cache [size nk] [assoc] [blocksize] [repl]
*
****************************************************************************/

#include <iostream>
#include <string>
#include <random>
#include <cstring>

using namespace std;

/*
 * Structure to store the cache information
 */
struct mem_cache {
    uint64_t* cache_tag;        														/* Stores the cache tag, including the dirty bit */
    uint64_t* cache_lru;        														/* Store the cache LRU counter registers, only used for LRU */
										
    explicit mem_cache(uint32_t array_size) {										
        cache_tag = new uint64_t[array_size];   										/* initialize all cache and lru indexes to zero */
        cache_lru = new uint64_t[array_size];										
        for(int i=0; i < array_size; i++) {										
            cache_tag[i] = cache_lru[i] = 0;										
        }										
    }										
};										
										
uint8_t numBits(uint64_t num);										
uint8_t numBits(uint64_t num) { 														/* Functions as Log2(n), giving us the number of bits used */
    uint8_t size = 0;
    while (num != 0){
        num = num >> 1;
        size++;
    }
    return size-1;
}

int main(int argc, char **argv) {

    /*
	 * Number of input arguments should be 4+1 
	 */
    if(argc != 5) {
        printf("Invalid arguments, please use this format\n\t :: ./cache [capacity nk] [assoc] [cache_block_size] [repl]\n");
        exit(1);    																	/* If not, throw an error to stdout */
    }				
				
    /* 				
	 * Converting the user input strings into numbers for storage 				
	 */				
    uint32_t cache_cap_nk        = strtol(argv[1], nullptr, 10);     					/* In KB with base 10 */
    uint32_t cache_assoc         = strtol(argv[2], nullptr, 10);     					/* Associativity of the cache */
    uint32_t cache_block_size    = strtol(argv[3], nullptr, 10);;    					/* Size of each cache block */
				
    if(cache_cap_nk < 1 || cache_assoc < 1 || cache_block_size < 1) {					/* Now, check validity of user input */
        printf("Invalid parameters nk: %s kB, assoc: %s or blocksize: %s\n All must be more than 1 \n", argv[1], argv[2], argv[3]);
        exit(1);
    }

    uint8_t replacement_policy;                                                         /* We will store user entered replacement policy */
    if(strcmp(argv[4], "l") == 0 || strcmp(argv[4], "L") == 0) {
        replacement_policy = 0;                                                         /* 0 if LRU, 1 if random */
    } else if(strcmp(argv[4], "r") == 0 || strcmp(argv[4], "R") == 0){
        replacement_policy = 1;
    } else{                                                                             /* otherwise, throw an error on stdout */
        printf("Invalid parameter 4 - %s, enter l for LRU or r for random \n", argv[4]);
        exit(1);
    }

    uint32_t lines_of_cache = (cache_cap_nk * 1024) / (cache_assoc * cache_block_size);
    if(lines_of_cache < 1) {                                                           /* In case the assoc * block_size exceeds cache_size */
        printf("Invalid cache dimensions, try again \n");
        exit(1);
    }

    uint8_t offset_bits = numBits(cache_block_size);                                    /* Number of bits to ignore */

    const uint64_t dirty_bit = 0x8000000000000000;                                      /* This is our dirty bit, MSB of address */

    unsigned seed = time(nullptr);
    mt19937 rand_num(seed);                                                             /* Seed for truly random number generation */

    auto *index = new uint64_t[lines_of_cache];                                         /* This is the index counter we will use for LRU */
    mem_cache new_cache = mem_cache(cache_assoc*lines_of_cache);                		/* The main DS for all cache storage purposes */

    for(int i=0; i<lines_of_cache; i++)                                                 /* Initialize index counter for each line */
        index[i] = 0;

    uint32_t total_num_access[3] = {0, 0, 0}, total_num_miss[3] = {0, 0, 0};            /* We store {READS, WRITES and TOTAL} for all_access and all_misses */
    for(uint32_t i=0; i<cache_assoc*lines_of_cache; i++) {
        new_cache.cache_tag[i] = new_cache.cache_lru[i] = 0;
    }
    string lineInput;
    for (uint64_t line_num = 0; line_num < 1000000000000; line_num++) {                 /* Precautionary 1 trillion lines of max limit */
        if (!getline(cin,lineInput)) {
            break;                                                                      /* Break when EOF */
        }

        if (lineInput[0] != 'r' && lineInput[0] != 'w') {                               /* Is it a read or write command... */
            printf("Error reading input line %s",
                   lineInput.c_str());                                                  /* if neither, throw an error */
            break;
        }
        int read_or_write = (lineInput[0] == 'r') ? 0 : 1;                              /* Read or write ?? */

        /*
         * Below snippet converts address hex values such as "0x78dc56" into a numeric equivalents,
         * so, "0x78dc56" becomes 0x78dc56 ==> 7920726 (upto 64 bits)
         */
        string address_string = lineInput.substr(2);
        unsigned int len = address_string.length();
        uint64_t hex_address = 0;
        for (int i = 0; i < len; i += 4) {
            int shift = ((i + 4) > len) ? (int) len - i : 4;
            hex_address = (hex_address << (shift * 4)) + stoi(address_string.substr(i, 4), nullptr, 16);
        }
        /**
         *  The address of value is stored in the hex_address variable now
         */

        uint64_t cache_line_num = (hex_address >> offset_bits) % lines_of_cache;
        uint64_t tag_value = hex_address >> (offset_bits + numBits(lines_of_cache));    /* Remove offset and indexed bits before storing tag value */

        unsigned int set_index, index_lru;
        for (set_index = 0, index_lru = 0; set_index < cache_assoc; set_index++) {      /* Looping through the selected set to find our tag */
            /*******
             * CASE-1 : When the address tag is present in cache
             *******/
            if ((new_cache.cache_tag[cache_line_num * cache_assoc + set_index] & dirty_bit) == dirty_bit) {
                /** The dirty bit must be set before we search for tag in our cache line **/
                if (new_cache.cache_tag[cache_line_num * cache_assoc + set_index] == (tag_value | dirty_bit)) {
                    new_cache.cache_lru[cache_line_num * cache_assoc + set_index] = ++index[cache_line_num];
                    break;
                }
            }
            /*******
             * CASE-2 : When the dirty bit is not set, it means the cache needs to be populated first time
             *******/
            else {
                /*** compulsory miss ***/
                new_cache.cache_tag[cache_line_num * cache_assoc + set_index] = tag_value | dirty_bit;      /* Store the tag */
                new_cache.cache_lru[cache_line_num * cache_assoc + set_index] = ++index[cache_line_num];    /* Increment the LRU counter */
                total_num_miss[read_or_write]++;                                                            /* and, count this as a miss! */
                break;
            }
            /* Calculate the least recently used index into: index_lru */
            if (new_cache.cache_lru[cache_line_num * cache_assoc + set_index] <
                new_cache.cache_lru[cache_line_num * cache_assoc + index_lru]) {
                index_lru = set_index;
            }
        }
        /*******
         * CASE-3 : Our search has failed, and we have a miss. Need to replace an existing tag
         *******/
        if (set_index == cache_assoc) {                                                                     /* This means random replacement, */
            if( replacement_policy == 1) {
                index_lru = rand_num() % cache_assoc;                                                       /* calculate a random index */
            }
            new_cache.cache_tag[cache_line_num * cache_assoc + index_lru] = tag_value | dirty_bit;          /* Store the tag */
            new_cache.cache_lru[cache_line_num * cache_assoc + index_lru] = ++index[cache_line_num];        /* and the LRU counter */
            total_num_miss[read_or_write]++;                                                                /* Count it as a miss */
        }
        total_num_access[read_or_write]++;                                                                  /* Always increment no of accesses */
    }

    total_num_access[2] = total_num_access[0] + total_num_access[1];                                        /* Total = Read + Write */
    total_num_miss[2]   = total_num_miss[0] + total_num_miss[1];
    printf("%u %0.6f%% %u %0.6f%% %u %0.6f%% \n",                                                   		/* Now print the final results */
           total_num_miss[2], (double)(total_num_miss[2]*100.0)/total_num_access[2],
           total_num_miss[0], (double)(total_num_miss[0]*100.0)/total_num_access[0],
           total_num_miss[1], (double)(total_num_miss[1]*100.0)/total_num_access[1]);

    /*
     **************** The END ****************
     */
}