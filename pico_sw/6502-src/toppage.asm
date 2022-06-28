include "portdefs.asm"

const BANK_SIZE_KIB = 16
const BANK_SIZE = BANK_SIZE_KIB * 1024

section code, "reset code", BANK_SIZE - 256, BANK_SIZE - 6
{
    section code, "reset code remap", 65536 - 256
    {
        subroutine initSystem
        {
            //disable irq
            sei
            // init stack
            ldx     #$FF
            txs
            //init bank registers
            lda     #0
            sta     PORT_BANK_0
            ina
            sta     PORT_BANK_1
            ina
            sta     PORT_BANK_2
            ina
            sta     PORT_BANK_3
            //start ehBasic
            jmp     $C000
        }
        
        // ****************
        subroutine noAction
        {
            rti
        }
    }
}

section code, "vectors", BANK_SIZE - 6, BANK_SIZE
{
    section code, "vectors remap", 65536 - 6, 65536
    {
        // system vectors
        define word = noAction         // NMI vector
        define word = initSystem       // RESET vector
        define word = noAction         // IRQ vector
    }
}
