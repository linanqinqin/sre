#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/kvm_host.h>

// Dummy function for SRE injection (to be implemented later)
static void inject_sre(struct kvm_vcpu *vcpu, gpa_t gpa) {
    // Empty for now
}

// Pre-handler: Runs BEFORE kvm_mmu_page_fault
static int handler_pre(struct kprobe *p, struct pt_regs *regs) {
    // Extract arguments from registers (x86_64 calling convention)
    struct kvm_vcpu *vcpu = (struct kvm_vcpu *)regs->di;
    gpa_t gpa = (gpa_t)regs->si;
    // there are two more arguments in the orginal kvm_mmu_page_fault, 
    // void *insn, int insn_len
    // might need them in the future

    // Log the access and call inject_sre unconditionally
    pr_info("PRE: vCPU=%p accessed GPA=0x%llx\n", vcpu, gpa);

    // the per-GPA metadata determines whether this is an SRE
    if (1) {
        inject_sre(vcpu, gpa);
        // if this does not contain an EPT violation, skip kvm_mmu_page_fault
        // return 1;    
    }

    // Allow original kvm_mmu_page_fault to execute
    return 0;
}

// Post-handler: Runs AFTER kvm_mmu_page_fault
static void handler_post(struct kprobe *p, struct pt_regs *regs, unsigned long flags) {
    struct kvm_vcpu *vcpu = (struct kvm_vcpu *)regs->di;
    gpa_t gpa = (gpa_t)regs->si;

    // Log completion and call inject_sre again
    pr_info("POST: vCPU=%p GPA=0x%llx handled\n", vcpu, gpa);

    // only injects sre in post if this contains both EPT violation and SRE
    if (1) {
        inject_sre(vcpu, gpa);
    }
}

// Kprobe configuration
static struct kprobe kp = {
    .symbol_name = "kvm_mmu_page_fault", // Function to hook
    .pre_handler  = handler_pre,         // Pre-handler
    .post_handler = handler_post         // Post-handler
};

// Module initialization
static int __init sre_init(void) {
    int ret = register_kprobe(&kp);
    if (ret < 0) {
        pr_err("Failed to register kprobe: %d\n", ret);
        return ret;
    }
    pr_info("SRE: Kprobe registered successfully\n");
    return 0;
}

// Module cleanup
static void __exit sre_exit(void) {
    unregister_kprobe(&kp);
    pr_info("SRE: Kprobe unregistered\n");
}

module_init(sre_init);
module_exit(sre_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("linanqinqin");
MODULE_DESCRIPTION("SRE Emulation via Kprobes");
