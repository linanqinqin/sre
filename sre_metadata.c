#include "sre_metadata.h"
#include <linux/slab.h>  // For kmalloc/kfree

// Define hash table and spinlock
DEFINE_HASHTABLE(sre_metadata_hash, SRE_HASH_BITS);
DEFINE_SPINLOCK(sre_flags_spinlock);

// Add a GPA to the hash table
void sre_flags_new(gpa_t gpa) {
    struct sre_flags *entry = kmalloc(sizeof(*entry), GFP_KERNEL);
    if (!entry) return;

    entry->gpa = gpa;
    entry->is_sre = false;          // is_sre should be initialized to false; the flash emulation layer sets it later 
    entry->is_ept = true;           // is_ept should be initialized to true, since the first occurence is always a regular EPT violation 
    atomic_set(&entry->access_count, 0);    // probably does not need atomic here at init

    spin_lock(&sre_flags_spinlock);
    hash_add(sre_metadata_hash, &entry->node, gpa);
    spin_unlock(&sre_flags_spinlock);
}

// Remove a GPA from the hash table
void sre_flgas_remove(gpa_t gpa) {
    struct sre_flags *entry;
    hash_for_each_possible(sre_metadata_hash, entry, node, gpa) {
        if (entry->gpa == gpa) {
            spin_lock(&sre_flags_spinlock);
            hash_del(&entry->node);
            spin_unlock(&sre_flags_spinlock);
            kfree(entry);
            break;
        }
    }
}

// Lookup metadata for a GPA
struct sre_flags *sre_flags_lookup(gpa_t gpa) {
    struct sre_flags *entry;
    hash_for_each_possible(sre_metadata_hash, entry, node, gpa) {
        if (entry->gpa == gpa) {
            return entry;
        }
    }
    return NULL;
}