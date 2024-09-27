/** @file
 * @brief Crash dumper
 *
 * This crash dumper will generate a Zephyr core dump:
 * https://docs.zephyrproject.org/latest/services/debugging/coredump.html#file-format
 */


#include <stdint.h>
#include "FreeRTOS.h"
#include "CrashCatcher.h"

#include "dfm_demo_config.h"

#if (DEMO_CFG_USE_ZEPHYR_CORE_DUMP_FORMAT == 1)

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define ARCH_DATA_SIZE  (17 * 4) /* (9 * 4) */

#define TARGET_CODE_X86             1 // Unsupported
#define TARGET_CODE_X86_64          2 // Unsupported
#define TARGET_CODE_ARM_CORTEX_M    3
#define TARGET_CODE_RISC_V          4 // Unsupported
#define TARGET_CODE_XTENSA          5 // Unsupported
#define TARGET_CODE_ARM64           6 // Unsupported

#define U_CRASH_DUMP_TARGET_CODE    TARGET_CODE_ARM_CORTEX_M

#ifndef U_CRASH_DUMP_TARGET_CODE
# error "You must define the target code. Please see TARGET_CODE_xxx"
#endif

#if U_CRASH_DUMP_TARGET_CODE == TARGET_CODE_ARM_CORTEX_M
#include "stm32l4xx.h"
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC PROTOTYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

// The Dfm framework expects us to run CrashCatcher which uses a
// stack defined in this variable. To make it compile without modifications
// we just define a dummy variable here.
uint32_t g_crashCatcherStack[1];

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */


static void dumpMemoryBlock(uint32_t startAddr, uint32_t lengthBytes)
{
    uint32_t endAddr = startAddr + lengthBytes;
    uint32_t *pPtr = (uint32_t *)startAddr;
    static const uint8_t memHdr[] = {
        'M',  // Block ID: Arch
        1, 0, // Header version
    };
    CrashCatcher_DumpMemory(&memHdr, CRASH_CATCHER_BYTE, sizeof(memHdr));
    CrashCatcher_DumpMemory(&startAddr, CRASH_CATCHER_WORD, 1);
    CrashCatcher_DumpMemory(&endAddr, CRASH_CATCHER_WORD, 1);
    CrashCatcher_DumpMemory(pPtr, CRASH_CATCHER_BYTE, lengthBytes);
}

static void dumpExtraArchData(void)
{
#if U_CRASH_DUMP_TARGET_CODE == TARGET_CODE_ARM_CORTEX_M
    dumpMemoryBlock((uint32_t)&SCB->CFSR, 4 * 6); // This will cover the 6 Cortex M fault regs
#endif
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

typedef struct {
		struct {
			uint32_t	r0;
			uint32_t	r1;
			uint32_t	r2;
			uint32_t	r3;
			uint32_t	r12;
			uint32_t	lr;
			uint32_t	pc;
			uint32_t	xpsr;
			uint32_t	sp;

			// Below is populated by asm code in DFM_Fault_Handler
			uint32_t	r4;  // 36
			uint32_t	r5;  // 40
			uint32_t	r6;  // 44
			uint32_t	r7;  // 48
			uint32_t	r8;  // 52
			uint32_t	r9;  // 56
			uint32_t	r10; // 60
			uint32_t	r11; // 64
		} r;
	} __packed arm_regs_t;

arm_regs_t regs = {0};

void DFM_Fault_Handler( void ) __attribute__ (( naked, aligned(8) ));
void z_core_dump(uint32_t stackPointer, arm_regs_t* pArchData);


void z_core_dump(uint32_t stackPointer, arm_regs_t* pArchData)
{
    uint32_t fatalReason = 0; // Set to "K_ERR_CPU_EXCEPTION" for now
    CrashCatcherInfo info = {
        .isBKPT = 0,
        .sp = stackPointer
    };
    // See https://docs.zephyrproject.org/latest/services/debugging/coredump.html
    static const uint8_t fileHdr[] = {
        'Z', 'E', // Signature
        1, 0, // Header version
        U_CRASH_DUMP_TARGET_CODE, 0, // Target code
        5,    // Pointer size (2^x bits): 32 bits
        0     // Flags
    };
#ifdef TARGET_CODE_ARM_CORTEX_M
    static const uint8_t archHdr[] = {
        'A',  // Block ID: Arch
        2, 0, // Header version (JK: Changed 1 -> 2, it seems the parser supports two header versions, v1 (9 regs) and v2 (17 regs)
        ARCH_DATA_SIZE, 0, // Number of bytes (JK: Changed 9 -> 17)
    };
#else
#error "Unsupported target code"
#endif

    // TODO: This is currently needed for dfmCrashCatcher.
    //       Clean this up when we have found a better way of handling it.
    extern uint32_t g_crashCatcherStack[];
    g_crashCatcherStack[0] = CRASH_CATCHER_STACK_SENTINEL;

    CrashCatcher_DumpStart(&info);
    CrashCatcher_DumpMemory(&fileHdr, CRASH_CATCHER_BYTE, sizeof(fileHdr));
    CrashCatcher_DumpMemory(&fatalReason, CRASH_CATCHER_WORD, 1);
    CrashCatcher_DumpMemory(&archHdr, CRASH_CATCHER_BYTE, sizeof(archHdr));
    CrashCatcher_DumpMemory(pArchData, CRASH_CATCHER_BYTE, ARCH_DATA_SIZE);
    dumpMemoryBlock(stackPointer, 400);
    dumpExtraArchData();
    CrashCatcher_DumpEnd();
}


// FreeRTOS example fault handler, extended to also store r4-r11
void DFM_Fault_Handler(void)
{

    __asm volatile
    (
        " tst lr, #4                                                \n"
        " ite eq                                                    \n"
        " mrseq r0, msp                                             \n"
        " mrsne r0, psp                                             \n"

    	/* Store additional regs in regs struct*/
    	" ldr r1, =regs \n"
    	" str r4, [r1, #36] \n"
       	" str r5, [r1, #40] \n"
       	" str r6, [r1, #44] \n"
       	" str r7, [r1, #48] \n"
       	" str r8, [r1, #52] \n"
       	" str r9, [r1, #56] \n"
        " str r10, [r1, #60] \n"
        " str r11, [r1, #64] \n"

    	/* This line is unnecessary it seems, a second arg to prvGetRegistersFromStack that doesn't exist */
       /*  " ldr r1, [r0, #24]                                         \n" */

    	/* Call prvGetRegistersFromStack */
    	" ldr r2, handler2_address_const                            \n"
        " bx r2                                                     \n"
        " handler2_address_const: .word prvGetRegistersFromStack    \n"
    );
}

void prvGetRegistersFromStack( uint32_t *pulFaultStackAddress )
{


	/* Can perhaps be improved. If we push SP to the stack (R0 in DFM_Fault_Handler) we could simply cast pulFaultStackAddress to a arm_regs_t pointer */
	regs.r.r0 = (uint32_t)pulFaultStackAddress[ 0 ];
	regs.r.r1 = (uint32_t)pulFaultStackAddress[ 1 ];
	regs.r.r2 = (uint32_t)pulFaultStackAddress[ 2 ];
	regs.r.r3 = (uint32_t)pulFaultStackAddress[ 3 ];
	regs.r.r12 = (uint32_t)pulFaultStackAddress[ 4 ];
	regs.r.lr = (uint32_t)pulFaultStackAddress[ 5 ];
	regs.r.pc = (uint32_t)pulFaultStackAddress[ 6 ];
    regs.r.xpsr = (uint32_t)pulFaultStackAddress[ 7 ];
    regs.r.sp = (uint32_t)pulFaultStackAddress;

    /* r4-r11 are populated by DFM_Fault_Handler (asm code) */

    printf("Fault handler\n");
    printf("  R0:  0x%08X\n", (unsigned int)regs.r.r0);
    printf("  R1:  0x%08X\n", (unsigned int)regs.r.r1);
    printf("  R2:  0x%08X\n", (unsigned int)regs.r.r2);
    printf("  R3:  0x%08X\n", (unsigned int)regs.r.r3);
    printf("  R4:  0x%08X\n", (unsigned int)regs.r.r4);
    printf("  R5:  0x%08X\n", (unsigned int)regs.r.r5);
    printf("  R6:  0x%08X\n", (unsigned int)regs.r.r6);
    printf("  R7:  0x%08X\n", (unsigned int)regs.r.r7);
    printf("  R8:  0x%08X\n", (unsigned int)regs.r.r8);
    printf("  R9:  0x%08X\n", (unsigned int)regs.r.r9);
    printf("  R10: 0x%08X\n", (unsigned int)regs.r.r10);
    printf("  R11: 0x%08X\n", (unsigned int)regs.r.r11);
    printf("  R12: 0x%08X\n", (unsigned int)regs.r.r12);
    printf("  LR:  0x%08X\n", (unsigned int)regs.r.lr);
    printf("  PC:  0x%08X\n", (unsigned int)regs.r.pc);
    printf("  PSR: 0x%08X\n", (unsigned int)regs.r.xpsr);
    printf("  SP:  0x%08X\n", (unsigned int)regs.r.sp);
    printf("\nRestarting\n");

    z_core_dump((uint32_t)pulFaultStackAddress, &regs);
}

#endif
