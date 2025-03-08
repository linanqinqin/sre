#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/kvm_host.h>

#include "sre_metadata.h"

// Dummy function for SRE injection (to be implemented later)
static void emulate_sre(struct kvm_vcpu *vcpu, gpa_t gpa) {
    // Empty for now

    // input arguments should probably include struct sre_flags
}

/**
 * *********************************** * 
 * kprobe hooks for kvm_mmu_page_fault * 
 * *********************************** * 
 */ 

// Pre-handler: Runs BEFORE kvm_mmu_page_fault
static int ept_violation_pre(struct kprobe *p, struct pt_regs *regs) {
    // Extract arguments from registers (x86_64 calling convention)
    struct kvm_vcpu *vcpu = (struct kvm_vcpu *)regs->di;
    gpa_t gpa = (gpa_t)regs->si;
    // there are two more arguments in the orginal kvm_mmu_page_fault, 
    // void *insn, int insn_len
    // might need them in the future

    struct sre_flags *sflags; 

    // Log the access and call emulate_sre unconditionally
    pr_info("[linanqinqin] PRE: vCPU=%p accessed GPA=0x%llx\n", vcpu, gpa);

    // lookup the per-GPA SRE flags metadata
    sflags = sre_flags_lookup(gpa);
    if (!sflags) return 0;      // skip ept_violation_pre when lookup failed, due to allocation failure 

    // checking the ept and sre flags to determine appropriate paths 
    if (sflags->is_ept) {
        // this EPT violation contains a regular EPT violation
        
        // unset the ept flag, but be cautious: what if kvm_mmu_page_fault could not resolve the violation?
        // unsetting the ept flag would disable regular violation handling, which is intended under the 
        // assumption that kvm_mmu_page_fault will always succeed
        sflags->is_ept = false; 
        
        // directly proceed to kvm_mmu_page_fault 
        return 0;
    }
    else if (sflags->is_sre) {
        // this EPT violation is only for SRE
        
        // unset the sre flag
        sflags->is_sre = false;
        // inject an SRE to the guest OS
        emulate_sre(vcpu, gpa);
        
        // return non-0 to skip kvm_mmu_page_fault
        return 1;
    } 
    else {
        // this should not happen
    }

    // default: allow original kvm_mmu_page_fault to execute
    return 0;
}

// Post-handler: Runs AFTER kvm_mmu_page_fault
static void ept_violation_post(struct kprobe *p, struct pt_regs *regs, unsigned long flags) {
    struct kvm_vcpu *vcpu = (struct kvm_vcpu *)regs->di;
    gpa_t gpa = (gpa_t)regs->si;
    struct sre_flags *sflags; 

    // Log completion and call emulate_sre again
    pr_info("[linanqinqin] POST: vCPU=%p GPA=0x%llx handled\n", vcpu, gpa);

    // lookup the per-GPA SRE flags metadata
    sflags = sre_flags_lookup(gpa); 
    if (!sflags) return;      // allocation failure 

    // only injects sre in post if this contains both EPT violation and SRE
    if (sflags->is_sre) {
        // this is when both ept and sre were set, and the regular EPT violation was handled first
        sflags->is_sre = false;
        emulate_sre(vcpu, gpa);
    }
}

// Kprobe configuration for kvm_mmu_page_fault: kvm's EPT violation handler
static struct kprobe ept_violation_kp = {
    .symbol_name = "kvm_mmu_page_fault", // Function to hook
    .pre_handler  = ept_violation_pre,         // Pre-handler
    .post_handler = ept_violation_post         // Post-handler
}; 


/**
 * ************************************ * 
 * kprobe hooks for kvm_zap_gfn_range * 
 * ************************************ * 
 */ 

// Pre-handler for kvm_zap_gfn_range
static int ept_invalidation_pre(struct kprobe *p, struct pt_regs *regs) {
    struct kvm *kvm = (struct kvm *)regs->di;
    gfn_t start_gfn = (gfn_t)regs->si;
    gfn_t end_gfn = (gfn_t)regs->dx;
    gfn_t gfn_idx

    // Iterate through GFNs and mark is_ept=true
    for (gfn_idx = start_gfn; gfn_idx < end_gfn; gfn_idx++) {
        gpa_t gpa = gfn_to_gpa(gfn_idx); // Convert GFN to GPA
        struct sre_flags *entry = sre_flags_lookup(gpa);
        if (entry) {
            entry->is_ept = true; // Mark as KVM-triggered EPT invalidation
        }
    }

    return 0;
}

// Kprobe for kvm_zap_gfn_range: invalidate an EPT entry 
static struct kprobe ept_invalidation_kp = {
    .symbol_name = "kvm_zap_gfn_range", 
    .pre_handler = ept_invalidation_pre
};


// Module initialization
static int __init sre_init(void) {
    int ret; 

    ret = register_kprobe(&ept_violation_kp);
    if (ret < 0) {
        pr_err("[linanqinqin] Failed to register kprobe kvm_mmu_page_fault: %d\n", ret);
        return ret;
    }

    ret = register_kprobe(&ept_invalidation_kp);
    if (ret < 0) {
        pr_err("[linanqinqin] Failed to register kprobe kvm_zap_gfn_range: %d\n", ret);
        return ret;
    }

    // initialize the sre_flags hashmap 
    sre_flags_init(); 

    pr_info("[linanqinqin] SRE Kprobe registered successfully\n");
    return 0;
}

// Module cleanup
static void __exit sre_exit(void) {
    unregister_kprobe(&ept_violation_kp);
    sre_flags_cleanup();
    pr_info("[linanqinqin] SRE Kprobe unregistered\n");
}

module_init(sre_init);
module_exit(sre_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("linanqinqin");
MODULE_DESCRIPTION("SRE Emulation via Kprobes");
