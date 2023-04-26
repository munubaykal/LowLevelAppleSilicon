

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "Hypervisor/Hypervisor.h"

#define VM_MEM_SIZE (1024*1024)
#define ASSERT(x) assert(x == HV_SUCCESS)

void * vm_mem;
uint64_t x0 = 0;
uint64_t x8 = 0;
uint64_t x9 = 0;
uint64_t pc = 0;
typedef union esr_el2{
    uint64_t value;
    struct {
        uint64_t iss : 25;
        uint64_t ic : 1;
        uint64_t ec :6;
    };
} esr_el2;

typedef union esr_el2_iss{
    uint64_t value;
    struct {
        uint64_t res0 : 6;
        uint64_t WnR : 1;
        uint64_t res1 : 9;
        uint64_t SRT :5;
    };
} esr_el2_iss;

int main(int argc, const char * argv[]) {
    
    uint64_t sizeoo = sizeof(esr_el2);
    
    volatile hv_return_t vm = hv_vm_create(NULL);
    
    ASSERT(vm);
    
    posix_memalign(&vm_mem, 0x1000, VM_MEM_SIZE);
    hv_vm_map(vm_mem, 0x0000000, VM_MEM_SIZE,
              HV_MEMORY_READ |
              HV_MEMORY_WRITE |
              HV_MEMORY_EXEC);
    hv_vcpu_t cpu0;
    hv_vcpu_exit_t * exit_reason;
    hv_return_t vcpu_created = hv_vcpu_create(&cpu0, &exit_reason, NULL);

    const uint32_t s_ckVMCode[] = {
        0xD28EF100,// movz x0, 0x7788
        0xF2AAACC0,// movk x0, 0x5566, lsl 16
        0xD2A01201,// MOV X1, #0x900000
        0xD2800025,// MOV X5, #1
        0xF9400002,// LDR x2, [X0]
        0x8B050043,// add X3, x2, x5
        0xF9000023,// str x3, [X1]
        0xD4000002,// hvc #0


    };
    
    memset(vm_mem, 0, VM_MEM_SIZE);
    memcpy(vm_mem, s_ckVMCode, sizeof(s_ckVMCode));
    ASSERT(hv_vcpu_set_reg(cpu0, HV_REG_CPSR, 0x3c4));
    ASSERT(hv_vcpu_set_reg(cpu0, HV_REG_PC, 0x0000000));

    
    while (true) {
        ASSERT(hv_vcpu_run(cpu0));
        if(exit_reason->reason == HV_EXIT_REASON_EXCEPTION )
        {
            esr_el2 reg;
            esr_el2_iss reg2;
            reg.value = (exit_reason->exception.syndrome);
            bool is_mmio = reg.ec == 0x24;
            if(is_mmio ){
                reg2.value = reg.iss;
                uint64_t temp = 0;
                if(reg2.WnR){
                    ASSERT(hv_vcpu_get_reg(cpu0, reg2.SRT, &temp));
                    printf("WRITE -> ADDR : %x | VALUE : %x\n", (uint64_t)(exit_reason->exception.virtual_address), temp );
                }else{
                    temp  = 0xCAFE;
                    ASSERT(hv_vcpu_set_reg(cpu0, reg2.SRT, temp));
                    printf("READ -> ADDR : %x | VALUE : %x\n", (uint64_t)(exit_reason->exception.virtual_address), temp  );
                }
                ASSERT(hv_vcpu_get_reg(cpu0, HV_REG_PC, &pc));
                ASSERT(hv_vcpu_set_reg(cpu0, HV_REG_PC, pc+4));
                
            }
            
        }
        
    }
    
    
    
    
    ASSERT(vcpu_created);
    
    
    
    ASSERT(hv_vcpu_destroy(cpu0));
    ASSERT(hv_vm_destroy());
    
    printf("END OF HYPERVISOR APP\n");
    return 0;
}
