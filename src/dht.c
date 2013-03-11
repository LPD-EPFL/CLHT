
#include <math.h>
#ifdef __sparc__
#include "../include/dht.h"
#else
#include "dht.h"
#endif

inline int is_power_of_two (unsigned int x) {
    return ((x != 0) && !(x & (x - 1)));
}

inline int is_odd (int x) {
    return x & 1;
}

/** Jenkins' hash function for 64-bit integers. */
inline uint64_t __ac_Jenkins_hash_64( uint64_t key )
{
    key += ~(key << 32);
    key ^= (key >> 22);
    key += ~(key << 13);
    key ^= (key >> 8);
    key += (key << 3);
    key ^= (key >> 15);
    key += ~(key << 27);
    key ^= (key >> 31);
    return key;
}

/* Create a new bucket. */
bucket_t *create_bucket( int bucket_id ) {
	
    bucket_t *bucket = NULL;
    
	if( ( bucket = malloc( sizeof( bucket_t ) ) ) == NULL ) {
		return NULL;
	}
    
    bucket->bucket_id = bucket_id;
    
	if( ( bucket->entries = malloc( sizeof( entry ) * ENTRIES_PER_BUCKET ) ) == NULL ) {
		return NULL;
	}
    
    int j;
    for( j = 0; j < ENTRIES_PER_BUCKET; j++ ) {
        bucket->entries[ j ] = NULL;
    }
    
	bucket->next = NULL;
    
	return bucket;
}

int *num_buckets;
/* Create a new hashtable. */
hashtable_t *ht_create( int capacity ) {
    
	hashtable_t *hashtable = NULL;
    
	if( capacity < 1) return NULL;
    
	/* Allocate the table itself. */
	if( ( hashtable = malloc( sizeof( hashtable_t ) ) ) == NULL ) {
		return NULL;
	}
    
    /* Allocate pointers to the head nodes. */
	if( ( hashtable->table = malloc( sizeof( bucket_t * ) * capacity) ) == NULL ) {
		return NULL;
	}
    
    int i;
	for( i = 0; i < capacity; i++ ) {
        hashtable->table[ i ] = create_bucket(0);
        (hashtable->table[ i ])->next = create_bucket(1);
    }
    
	hashtable->capacity = capacity;
    
    if( ( num_buckets = calloc( capacity, sizeof( int ) ) ) == NULL ) {
		return NULL;
	}
    
	return hashtable;
}

/* Hash a key for a particular hash table. */
int ht_hash( hashtable_t *hashtable, uint64_t key ) {
    
	uint64_t hashval;
	hashval = __ac_Jenkins_hash_64(key);
    
	return hashval % hashtable->capacity;
}

/* Insert a key-value entry into a hash table. */
int ht_put( hashtable_t *hashtable, uint64_t key, void *value, int bin, int payload_size ) {
    
    //int bin;
    bucket_t *bucket = hashtable->table[ bin ];
    
    int j;
    do {
        for( j = 0; j < ENTRIES_PER_BUCKET; j++ ) {
            
            if(bucket->entries[j] == key) {
                    /* There's already a entry. */
                    /* MCORE_shfree((bucket->next)->entries[j]); */
                    /*   (bucket->next)->entries[j] = value; */
                    return false;
	    }
            else if (bucket->entries[j] == NULL) {
                /* Nope, could't find it. Time to insert a entry. */
                bucket->entries[j] = key;
                (bucket->next)->entries[j] = value;
                return true;
            }
        }
        
        if((bucket->next)->next == NULL) {
            unsigned int bucket_id = bucket->bucket_id;
            
            unsigned int b;
            bucket_t *expanding_bucket = bucket;
            unsigned int bid = (bucket_id >> 1);
            
            for( b = 0; b <= bid ; b++) {
                (expanding_bucket->next)->next = create_bucket(bucket_id + (b << 1) + 2);
                ((expanding_bucket->next)->next)->next = create_bucket(bucket_id + (b << 1) + 3);
                expanding_bucket = (expanding_bucket->next)->next;
            }
            
            num_buckets[bin] = ((bucket_id + 2) << 1);
        }
        bucket = (bucket->next)->next;
    } while (true);
}

/* Retrieve a key-value entry from a hash table. */
void * ht_get( hashtable_t *hashtable, uint64_t key, int bin ) {
    
  //int bin;
  bucket_t *bucket = NULL;
    
  //bin = ht_hash( hashtable, key );
  bucket = hashtable->table[ bin ];
    
  int j;
  do {
    for( j = 0; j < ENTRIES_PER_BUCKET; j++ ) {
            
      if(bucket->entries[j] == key) {
	/* Yes, found it. */
	return (bucket->next)->entries[j];
      } 
      else if (bucket->entries[j] == NULL) {
	return NULL;
    }
  }
        
  bucket = (bucket->next)->next;
        
} while (bucket != NULL);
    
    return;
}

/* Remove a key-value entry from a hash table. */
int ht_remove( hashtable_t *hashtable, uint64_t key, int bin ) {
    
	//int bin;
    bucket_t *bucket_x = NULL, *bucket_c = NULL, *bucket_p = NULL;
    
    //bin = ht_hash( hashtable, key );
    
    bucket_x = hashtable->table[ bin ];
    bucket_c = hashtable->table[ bin ];
    bucket_p = NULL;
    
    int j, k;
    do {
        for( j = 0; j < ENTRIES_PER_BUCKET; j++ ) {
            
            if(bucket_x->entries[j] != NULL) {
                if(bucket_x->entries[j] == key) {
                    /* Yes, found it. */
                    
                    k = j + 1;
                    do {
                        for(; k < ENTRIES_PER_BUCKET; k++ ) {
                            
                            if(bucket_c->entries[k] == NULL) {
                                
                                if(k == 0) {
                                    bucket_x->entries[j] = bucket_p->entries[ENTRIES_PER_BUCKET - 1];
                                    (bucket_x->next)->entries[j] = (bucket_p->next)->entries[ENTRIES_PER_BUCKET - 1];
                                    
                                    bucket_p->entries[ENTRIES_PER_BUCKET - 1] = NULL;
                                    (bucket_p->next)->entries[ENTRIES_PER_BUCKET - 1] = NULL; //free data itself
                                    
                                    if(ENTRIES_PER_BUCKET == 1) {
                                        int bucket_id = bucket_c->bucket_id;
                                        
                                        if(bucket_id < num_buckets[bin] >> 1 + num_buckets[bin] >> 2) {
                                            
                                            bucket_x = bucket_p;
                                            
                                            int delete = false;
                                            do {
                                                bucket_id = bucket_c->bucket_id;
                                                
                                                if(is_power_of_two(bucket_id)) {
                                                    delete = true;
                                                }
                                                
                                                bucket_p = bucket_c;
                                                bucket_c = (bucket_p->next)->next;
                                                
                                                if(delete) {
                                                    free(bucket_p->next->entries);
                                                    free(bucket_p->entries);
                                                    free(bucket_p->next);
                                                    free(bucket_p);
                                                }
                                            } while (bucket_c != NULL);
                                            
                                            (bucket_x->next)->next = NULL;
                                        }
                                        
                                        num_buckets[bin] = num_buckets[bin] >> 1;
                                    }
                                    
                                } else {
                                    bucket_x->entries[j] = bucket_c->entries[k - 1];
                                    (bucket_x->next)->entries[j] = (bucket_c->next)->entries[k - 1];
                                    
                                    bucket_c->entries[k - 1] = NULL;
                                    (bucket_c->next)->entries[k - 1] = NULL;
                                    
                                    if(k == 1) {
                                        int bucket_id = bucket_c->bucket_id;
                                        
                                        if(bucket_id < num_buckets[bin] >> 1 + num_buckets[bin] >> 2) {
                                            
                                            bucket_x = bucket_p;
                                            
                                            int delete = false;
                                            do {
                                                bucket_id = bucket_c->bucket_id;
                                                
                                                if(is_power_of_two(bucket_id)) {
                                                    delete = true;
                                                }
                                                
                                                bucket_p = bucket_c;
                                                bucket_c = (bucket_p->next)->next;
                                                
                                                if(delete) {
                                                    free(bucket_p->next->entries);
                                                    free(bucket_p->entries);
                                                    free(bucket_p->next);
                                                    free(bucket_p);
                                                }
                                            } while (bucket_c != NULL);
                                            
                                            (bucket_x->next)->next = NULL;
                                        }
                                        
                                        num_buckets[bin] = num_buckets[bin] >> 1;
                                    }
                                }
                                
                                return true;
                            }
                        }
                        
                        if((bucket_c->next)->next == NULL) {
                            bucket_x->entries[j] = bucket_c->entries[ENTRIES_PER_BUCKET - 1];
                            (bucket_x->next)->entries[j] = (bucket_c->next)->entries[ENTRIES_PER_BUCKET - 1];
                            
                            bucket_c->entries[ENTRIES_PER_BUCKET - 1] = NULL;
                            (bucket_c->next)->entries[ENTRIES_PER_BUCKET - 1] = NULL;
                            
                            return true;
                        } else {
                            bucket_p = bucket_c;
                            bucket_c = (bucket_p->next)->next;
                            k = 0;
                        }
                    } while (true);
                    
                    MCORE_shfree((bucket_x->next)->entries[j]);
                    (bucket_x->next)->entries[j] = NULL;
                }
            } else {
                
                return false;
            }
        }
        
        bucket_x = (bucket_x->next)->next;
        bucket_p = bucket_c;
        bucket_c = (bucket_p->next)->next;
        
    } while (bucket_x != NULL);
    
    /* Nope, could't find it. */
    
    return false;
}

void ht_destroy( hashtable_t *hashtable) {
    int capacity = hashtable->capacity;
    bucket_t *bucket_c = NULL, *bucket_p = NULL;
    
    int i,j;
    for( i = 0; i < capacity; i++ ) {
        
        bucket_c = hashtable->table[i];
        
        do {
            for( j = 0; j < ENTRIES_PER_BUCKET; j++ ) {
                
                if(bucket_c->entries[j] != NULL) {
                    
                    bucket_c->entries[j] = NULL;
                    (bucket_c->next)->entries[j] = NULL;
                }
            }
            
            bucket_p = bucket_c;
            bucket_c = (bucket_p->next)->next;
            
            free(bucket_p->next->entries);
            free(bucket_p->entries);
            free(bucket_p->next);
            free(bucket_p);
            
        } while (bucket_c != NULL);
	}
    
    free(hashtable->table);
    free(hashtable);
}

int ht_size( hashtable_t *hashtable, int capacity)
{
    int bin;
    bucket_t *bucket = NULL;
    
    size_t size = 0;
    
    for (bin = 0; bin < capacity; bin++)
    {
        bucket = hashtable->table[ bin ];
        
        uint8_t have_more = 1;
        
        int j;
        do
        {
            for( j = 0; j < ENTRIES_PER_BUCKET; j++ )
            {
                if(bucket->entries[j] != NULL)
                {
                    size++;
                } 
                else 
                {
                    have_more = 0;
                    break;
                }
            }
            bucket = (bucket->next)->next;
        } 
        while (have_more && bucket != NULL);
    }
    
    return size;
}
