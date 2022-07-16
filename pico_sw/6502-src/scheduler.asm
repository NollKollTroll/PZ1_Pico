include "portdefs.asm"
include "zpos.asm"


const RAM_SIZE_KIB = 512//4096
const BANK_SIZE_KIB = 16
const BANK_SIZE = BANK_SIZE_KIB * 1024
const NR_OF_BLOCKS = RAM_SIZE_KIB / BANK_SIZE_KIB
const NR_OF_BANKS = 64 / BANK_SIZE_KIB
const NR_OF_THREADS = 64

enum THREAD_STATE
{
    FREE = 0,
    ALLOCATED
}

enum BLOCK_OWNER
{
    SCHEDULER = 255,
    FREE = 254
}

section code, "dummy filler for ROM", 0, 512
{
    define byte[512] dummy_filler = {0, ...}
}

section bss, "zeropage reset", 0, 256
{
    reserve word source
    reserve word target
}

section code, "os code & data", 512, BANK_SIZE - 512
{
    section code, "os code & data remap", (BANK_SIZE * 3) + 512
    {
        //scheduler thread & memory allocation vars
        define byte[256] blockOwners = {BLOCK_OWNER.FREE, ...}
        define byte[NR_OF_THREADS] schStack = {0, ...}
        define byte[NR_OF_THREADS] schBank0 = {0, ...}
        define byte[NR_OF_THREADS] schSlotTime = {0, ...}
        define byte[NR_OF_THREADS] schNextThread = {0, ...}
        define byte[NR_OF_THREADS] threadState = {THREAD_STATE.FREE, ...}
        define byte schActiveThread = 0    

        define word [] OS_CALL_ADDR =
        {
            threadLoad,
            threadGetSlotTime,
            threadSetSlotTime,
            threadKill,
            threadGetBlocks,
            threadStateGet,
            threadSelfGet
        }        
        
        //out: osCallData[0] = calling thread nr
        subroutine threadSelfGet
        {
            lda     schActiveThread
            sta     osCallData[0]
            rts
        }

        //in:  osCallData[0] = thread number
        //out: osCallData[1] = thread state
        subroutine threadStateGet
        {
            ldy     osCallData[0]
            lda     threadState,y
            sta     osCallData[1]
            rts
        }

        //in: N/A
        //out: A = thread number,
        //     Carry = 0 if OK
        //           = 1 if fail
        subroutine threadAlloc
        {
            ldy     #0
            lda     #THREAD_STATE.FREE
            {
                cmp     threadState,y
                beq     .found
                iny
                cpy     #NR_OF_THREADS
                bne     @loop
                //no free thread found, set carry
                sec
                rts
            .found:
                //allocate thread
                lda     #THREAD_STATE.ALLOCATED
                sta     threadState,y
                //return: A = thread number, clear Carry
                tya
                clc
                rts
            }
        }

        //in:  A = thread number
        //out: Carry = 0 if OK
        //           = 1 if fail
        subroutine threadDealloc
        {
            tay
            lda     threadState,y
            cmp     #THREAD_STATE.ALLOCATED
            bne     .nok
            //set thread state
            lda     #THREAD_STATE.FREE
            sta     threadState,y
            //OK: clear carry
            clc
            rts
        .nok:
            //NOK: thread was not allocated, set carry
            sec
            rts
        }

        //in:  A = thread nr
        //out: A = block nr
        //     Carry = 0 if OK
        //           = 1 if fail
        subroutine blockAlloc
        {
            //save thread nr for later
            tax
            //counter, block 0 is reserved for scheduler
            ldy     #1
            lda     #BLOCK_OWNER.FREE
            {
                cmp     blockOwners,y
                beq     .found
                iny
                cpy     #(NR_OF_BLOCKS & 255)
                bne     @loop
                //no free block found, Carry set
                sec
                rts
            .found:
                //mark block allocated by thread nr
                txa
                sta     blockOwners,y
                //block nr in A
                tya
                //Carry clear
                clc
                rts
            }
        }

        //in:  A = block to dealloc
        subroutine blockDealloc
        {
            //mark block free
            tay
            lda     #BLOCK_OWNER.FREE
            sta     blockOwners,y
            rts
        }

        //in:  A = thread to dealloc
        subroutine blockDeallocThread
        {
            //thread nr in X for later
            tax
            //counter, block 0 is reserved for scheduler
            ldy     #1
            {
                cmp     blockOwners,y
                beq     .found
            .another:
                iny
                cpy     #(NR_OF_BLOCKS & 255)
                bne     @loop
                //return, all done
                rts
            .found:
                //mark block free
                lda     #BLOCK_OWNER.FREE
                sta     blockOwners,y
                //A = thread to remove
                txa
                jmp     .another
            }
        }

        //in:  osCallData[0] = thread nr
        //out: osCallData[1-15] = list of memory blocks, endmark #BLOCK_OWNER.FREE
        subroutine threadGetBlocks
        {
            //init result destination
            ldx     #0
            //thread nr
            lda     osCallData[0]
            ldy     #0
            {
                cmp     blockOwners,y
                bne     .next
                    tya
                    sta     osCallData[1],x
                    inx
                    lda     osCallData[0]
            .next:
                iny
                cpy     #(NR_OF_BLOCKS & 255)
                bne     @loop
            }
            lda     #BLOCK_OWNER.FREE
            sta     osCallData[1],x
            rts
        }

        //in:  osCallData[0] = thread nr
        subroutine threadKill
        {
            //thread nr
            lda     osCallData[0]
            //don't kill thread 0!
            beq     .end
            //dealloc thread
            sei
            jsr     threadDealloc
            //don't kill an already dead thread
            bcs     .end
            //search for previous thread
            ldx     #NR_OF_THREADS - 1
            lda     osCallData[0]
            {
                cmp     schNextThread,x
                beq     @continue
                dex
                bne     @loop
            }
            //unhook thread from scheduler
            lda     schNextThread,y
            sta     schNextThread,x
            //dealloc thread memory
            lda     osCallData[0]
            jsr     blockDeallocThread
        .end:
            cli
            rts
        }

        //in:  osCallData[0] = thread nr
        //     osCallData[1] = slot time
        subroutine threadSetSlotTime
        {
            //thread nr
            ldy     osCallData[0]
            //value to set
            lda     osCallData[1]
            //write value
            sta     schSlotTime,y
            rts
        }

        //in:  osCallData[0] = thread nr
        //out: osCallData[1] = slot time read
        subroutine threadGetSlotTime
        {
            //thread nr
            ldy     osCallData[0]
            //read value
            lda     schSlotTime,y
            //return it
            sta     osCallData[1]
            rts
        }

        subroutine threadLoad
        {
            //save bank 1&2
            lda     PORT_BANK_1
            pha
            lda     PORT_BANK_2
            pha
            //allocate new thread nr
            jsr     threadAlloc
            sta     threadNew
            //allocate block for bank 0 for new thread
            jsr     blockAlloc
            //init bank 1 to be able to write data for new thread
            sta     PORT_BANK_1
            //save bank 0 value for new thread
            //ignored in scheduler start-up of new thread,
            //only used in this routine
            sta     (BANK_SIZE * 1) + 512 - 10
            //do small or large memory alloc / init
            lda     osCallData[0]
            cmp     #1
            beq     .small
            {   //allocate 64KiB & save bank 1-3 values for new thread
                lda     threadNew
                jsr     blockAlloc
                sta     (BANK_SIZE * 1) + 512 - 9
                lda     threadNew
                jsr     blockAlloc
                sta     (BANK_SIZE * 1) + 512 - 8
                lda     threadNew
                jsr     blockAlloc
                sta     (BANK_SIZE * 1) + 512 - 7
                jmp     .load
            }
        .small:
            {   //allocate 16KiB & save bank 1-3 values for new thread
                //uses the same block for all banks
                lda     (BANK_SIZE * 1) + 512 - 10
                sta     (BANK_SIZE * 1) + 512 - 9
                sta     (BANK_SIZE * 1) + 512 - 7
                sta     (BANK_SIZE * 1) + 512 - 8
            }
        .load:
            //load thread binary
            jsr     loadS19
            //init A/X/Y/Flags values for new thread
            lda     #0
            sta     (BANK_SIZE * 1) + 512 - 6
            sta     (BANK_SIZE * 1) + 512 - 5
            sta     (BANK_SIZE * 1) + 512 - 4
            lda     #$20
            sta     (BANK_SIZE * 1) + 512 - 3
            //init start addr for new thread
            //the load subroutine sets the loadStartAddr
            lda     loadStartAddr.lo
            sta     (BANK_SIZE * 1) + 512 - 2
            lda     loadStartAddr.hi
            sta     (BANK_SIZE * 1) + 512 - 1
            //save bank 0 value for new thread
            lda     (BANK_SIZE * 1) + 512 - 10
            ldy     threadNew
            sta     schBank0,y
            //init stack pointer for new thread
            lda     #$ff - 9
            sta     schStack,y
            //set slot time for new thread
            lda     #10
            sta     schSlotTime,y
            //threadNew.next = thread0.next
            lda     schNextThread
            sta     schNextThread,y
            //thread0.next = threadNew
            sty     schNextThread
            //restore bank 1&2
            pla
            sta     PORT_BANK_2
            pla
            sta     PORT_BANK_1
            //return
            rts
        }

        //in:  A = hex-ascii byte
        //out: A = nybble 0-f
        //     Carry clear if OK, 
        //     Carry set if A not hex-ascii [0-9,A-F,a-f]
        subroutine convAsciiHex
        {
        .test_number:
            //test & convert if '0' - '9'
            cmp     #'0'
            bcc     .error
            cmp     #'9' + 1
            bcs     .test_upper
            sec
            sbc     #'0'
            clc
            rts
        .test_upper:
            //test & convert if 'A' - 'F'
            cmp     #'A'
            bcc     .error
            cmp     #'F' + 1
            bcs     .test_lower
            sec
            sbc     #'A' - 10
            clc
            rts
        .test_lower:
            //test & convert if 'a' - 'f'
            cmp     #'a'
            bcc     .error
            cmp     #'f' + 1
            bcs     .error
            sec
            sbc     #'a' - 10
            clc
            rts
        .error:
            //error, not hex-ascii
            sec
            rts
        }

        //out: acc = byte from serial monitor port
        //note: will wait until data arrives, no time-out
        subroutine getSerial1Byte
        {
            //wait for serial data ready
            {
                bit     PORT_SERIAL_0_FLAGS
                bvc     @loop
            }
            //read byte from serial port
            lda     PORT_SERIAL_0_IN
            rts
        }

        //reads 2 ascii-hex characters from serial port 1
        //out: A = 8-bit binary value from the 2 characters read
        subroutine getSerial1AsciiHexByte
        {
            //get nybble
            jsr     getSerial1Byte
            jsr     convAsciiHex
            bcs     .error
            //stuff data in hi nybble
            asl
            asl
            asl
            asl
            sta     hexTemp
            //get nybble
            jsr     getSerial1Byte
            jsr     convAsciiHex
            bcs     .error
            //combine nybbles
            adc     hexTemp
            //return
            rts
        .error:
            lda     #'E'
            sta     PORT_SERIAL_0_OUT
            jmp     *
            //sec
            //rts
        }

        subroutine loadS19
        {
            {
                declare .calc_addr
                //receive 'S'
                jsr     getSerial1Byte
                cmp     #'S'
                bne     @loop
                //receive '1' or '9'
                jsr     getSerial1Byte
                cmp     #'9'
                beq     .empty_buffer
                cmp     #'1'
                bne     @loop
                //S1-field
                //get count
                jsr     getSerial1AsciiHexByte
                sta     loadCount
                dec     loadCount
                dec     loadCount
                //skip last dec, because of dec after inc_done
                //dec     loadCount
                //get destination addr
                jsr     getSerial1AsciiHexByte
                sta     loadTargetAddr.hi
                jsr     getSerial1AsciiHexByte
                sta     loadTargetAddr.lo
                jmp     .calc_addr
                {
                        jsr     getSerial1AsciiHexByte
                        ldy     #0
                        sta     (loadTargetAddr),y
                        inc     loadTargetAddr.lo
                        bne     .inc_done
                        inc     loadTargetAddr.hi
                    .calc_addr:
                        lda     loadTargetAddr.hi
                        lsr
                        lsr
                        lsr
                        lsr
                        lsr
                        lsr
                        tay
                        lda     (BANK_SIZE * 1) + 512 - 10,y
                        sta     PORT_BANK_2
                        lda     loadTargetAddr.hi
                        and     #>(BANK_SIZE - 1)
                        clc
                        adc     #>(BANK_SIZE * 2)
                        sta     loadTargetAddr.hi
                    .inc_done:
                        dec     loadCount
                        bne     @loop
                }
                //skip checksum
                jmp     @loop
            }
        .empty_buffer:
            //S9-field
            //get & skip length
            jsr     getSerial1Byte
            jsr     getSerial1Byte
            //get & save start addr
            jsr     getSerial1AsciiHexByte
            sta     loadStartAddr.hi
            jsr     getSerial1AsciiHexByte
            sta     loadStartAddr.lo
            //skip checksum, just empty the serial buffer
            {
                jsr     getSerial1Byte
                cmp     #10
                bne     @loop
            }
            //done!
            rts
        }
        subroutine ehBasicThreadInit
        {
            //clear SID registers
            ldx     #$18
            {
                stz     PORT_SID_00,x
                dex
                bpl     @loop
            }
            
            //allocate thread for ehBasic
            jsr     threadAlloc
            sta     threadNew
            //allocate & save bank 0 value for ehBasic thread
            jsr     blockAlloc
            sta     schBank0
            //init bank 1 to be able to write values for the thread
            sta     PORT_BANK_1
            
            //copy EhBasic from ROM block 254 (in BANK_2) to newly allocated RAM block(in BANK_1)
            //skip first 512 byte (zp and stack areas)
            lda     #254
            sta     PORT_BANK_2
            lda     #(((BANK_SIZE * 2) + 512) >> 8)
            sta     source.hi
            stz     source.lo
            lda     #(((BANK_SIZE * 1) + 512) >> 8)
            sta     target.hi
            stz     target.lo
            
            ldy #0
			ldx #((BANK_SIZE - 512) >> 8)
			{
				{
					lda (source),y
					sta (target),y
					iny
					bne @loop
				}
				inc source + 1
				inc target + 1
				dex
				bne @loop
			}
            
            //allocate & save bank 1-3 value for ehBasic thread
            lda     threadNew
            jsr     blockAlloc
            sta     (BANK_SIZE * 1) + 512 - 9
            lda     threadNew
            jsr     blockAlloc
            sta     (BANK_SIZE * 1) + 512 - 8
            lda     threadNew
            jsr     blockAlloc
            sta     (BANK_SIZE * 1) + 512 - 7
            //init A/X/Y/Flags values for ehBasic thread
            lda     #0
            sta     (BANK_SIZE * 1) + 512 - 6
            sta     (BANK_SIZE * 1) + 512 - 5
            sta     (BANK_SIZE * 1) + 512 - 4
            lda     #$20
            sta     (BANK_SIZE * 1) + 512 - 3
            //init start addr for ehBasic thread
            lda     #<$200
            sta     (BANK_SIZE * 1) + 512 - 2
            lda     #>$200
            sta     (BANK_SIZE * 1) + 512 - 1
            //init stack pointer for ehBasic thread
            lda     #$ff - 9
            sta     schStack
            //init slot time for ehBasic thread
            lda     #40
            sta     schSlotTime
            //set next thread = 0
            lda     #0
            sta     schNextThread
            //init irq timer
            lda     #80
            sta     PORT_IRQ_TIMER_LO
            //start the thread
            lda     #0
            jmp     START_THREAD
        }
    }
}

section code, "scheduler code", BANK_SIZE - 256, BANK_SIZE - 6
{
    section code, "scheduler code remap", 65536 - 256
    {
        //call from active thread, NEVER from scheduler
        //uses active threads ZP & stack in bank 0
        //in:  A = os-routine number
        //     in-data in ZP
        subroutine osCall
        {
            //prepare to get osCallAddr
            asl
            tax
            //save thread bank 3
            lda     PORT_BANK_3
            pha
            //change to os bank 3
            stz     PORT_BANK_3
            //call the os routine with a little trick
            jsr     .jump
            //returns here after os routine
            //restore thread bank 3
            pla
            sta     PORT_BANK_3
            //return to thread
            rts
        .jump:
            jmp     (OS_CALL_ADDR, x)
        }

        // ****************
        subroutine initSystem
        {
            //disable irq
            sei
            // init monitor stack
            ldx     #$FF
            txs
            
            //copy scheduler from ROM block 255 (in BANK_2) to RAM block 0 (in BANK_1)
            //BANK_2 -> BANK_1
            //skip first 512 byte (zp and stack areas)
            lda     #255
            sta     PORT_BANK_2
            stz     PORT_BANK_1
            lda     #(((BANK_SIZE * 2) + 512) >> 8)
            sta     source.hi
            stz     source.lo
            lda     #(((BANK_SIZE * 1) + 512) >> 8)
            sta     target.hi
            stz     target.lo
            
            ldy #0
			ldx #((BANK_SIZE - 512) >> 8)
			{
				{
					lda (source),y
					sta (target),y
					iny
					bne @loop
				}
				inc source + 1
				inc target + 1
				dex
				bne @loop
			}
			
            //set banks for scheduler, memory block 0
            stz     PORT_BANK_0
            stz     PORT_BANK_3
            //init scheduler vars
            ldy     #NR_OF_THREADS - 1
            {
                lda     #THREAD_STATE.FREE
                sta     threadState,y
                lda     #BLOCK_OWNER.FREE
                sta     blockOwners,y
                dey
                bpl     @loop
            }
            //reserve memory block for scheduler
            lda     #BLOCK_OWNER.SCHEDULER
            sta     blockOwners[0]
            //init ehBasic
            jmp     ehBasicThreadInit
        }

        // ****************
        subroutine noAction
        {
            rti
        }

        // ****************
        subroutine scheduler
        {
            //save active registers
            pha                     //3
            phx                     //3
            phy                     //3 /9
            //save active bank regs
            lda     PORT_BANK_3     //4
            pha                     //3
            lda     PORT_BANK_2     //4
            pha                     //3
            lda     PORT_BANK_1     //4
            pha                     //3
            lda     PORT_BANK_0     //4 /25
            //select scheduler memory
            stz     PORT_BANK_3     //4
            //y = active thread
            ldy     schActiveThread //4
            sta     schBank0,y      //4
            //save active thread stack register
            tsx                     //2
            txa                     //2
            sta     schStack,y      //4
            //prepare next thread
            lda     schNextThread,y //4
        START_THREAD:
            sta     schActiveThread //3
            tay                     //2
            //init next thread slot time
            lda     schSlotTime,y   //4
            sta     PORT_IRQ_TIMER_HI //4
            //init next stack register
            ldx     schStack,y      //4
            txs                     //2 /43
            //restore next bank regs
            lda     schBank0,y      //4
            sta     PORT_BANK_0     //4
            pla                     //4
            sta     PORT_BANK_1     //4
            pla                     //4
            sta     PORT_BANK_2     //4
            pla                     //4
            sta     PORT_BANK_3     //4 /32
            //restore next registers
            ply                     //4
            plx                     //4
            pla                     //4
            //continue work in next thread
            rti                     //6 /18 == 127 + 7(irq)
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
        define word = scheduler        // IRQ vector
    }
}
