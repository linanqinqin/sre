#ifndef SRE_METADATA_H
#define SRE_METADATA_H

#include <linux/types.h>
#include <linux/hashtable.h>
#include <linux/spinlock.h>
#include <linux/kvm_host.h>  // For gpa_t

#define SRE_HASH_BITS 10

// Per-GPA metadata structure
struct sre_flags {
    gpa_t gpa;              // Guest Physical Address (key)
    bool is_sre;            // True if page is "slow" (flash-backed)
    bool is_ept;            // True if regular EPT violation
    // atomic_t access_count;  // For promotion/demotion policies
    struct hlist_node node; // Hash table linkage
};

// Declare hash table and spinlock (defined in sre_metadata.c)
extern DECLARE_HASHTABLE(sre_metadata_hash, SRE_HASH_BITS);
extern spinlock_t sre_hash_lock;

// internal helper functions
void sre_flags_new(gpa_t gpa);
void sre_flags_remove(gpa_t gpa);

// exposed APIs
void sre_flags_init(void);  
void sre_flags_cleanup(void); 
struct sre_flags *sre_flags_lookup(gpa_t gpa);

#endif /* SRE_METADATA_H */
