include "portdefs.asm"
include "zpos.asm"
include "oscalls.asm"

section bss, "ZeroPage", 0, 224
{
}

section code, "threadStack", 256, 512
{
}

const WAIT_LOOP = 65535

section code, "threadCode", 512
{    
    subroutine sidStart
    {
        jsr     sidLoad
        jsr     sidInit
        jmp     sidPlay
    }

    define byte[] sidFileName = {"ocean2.sid", 0}
    define byte[] errorMsg = {"Format error!\r\n", 0}    

    subroutine sidFormatError
    {
        ldy     #0
        {
            lda     errorMsg,y          //get next byte of filename            
            beq     @continue           //end if zero-termination found
            sta     PORT_SERIAL_0_OUT
            iny                         //increment index            
            jmp     @loop
        }
        jmp     @loop
    }

    subroutine sidLoad
    {
        // open file
        lda     #FILE_CMD.OPEN_R
        sta     PORT_FILE_CMD    
        ldy     #0
        {
            lda     sidFileName,y       //get next byte of filename            
            sta     PORT_FILE_DATA
            beq     @continue           //end if zero-termination found
            iny                         //increment index            
            jmp     @loop
        }
        //load sid-header
        //magicID
        lda     PORT_FILE_DATA  //header + $0
        cmp     #'P'
        bne     sidFormatError
        lda     PORT_FILE_DATA  //header + $1
        cmp     #'S'
        bne     sidFormatError
        lda     PORT_FILE_DATA  //header + $2
        cmp     #'I'
        bne     sidFormatError
        lda     PORT_FILE_DATA  //header + $3
        cmp     #'D'
        bne     sidFormatError
        //version
        lda     PORT_FILE_DATA  //header + $4    
        bne     sidFormatError
        lda     PORT_FILE_DATA  //header + $5    
        cmp     #2
        bne     sidFormatError
        //dataOffset
        lda     PORT_FILE_DATA  //header + $6
        bne     sidFormatError
        lda     PORT_FILE_DATA  //header + $7
        cmp     #$7C
        bne     sidFormatError
        //loadAddress
        lda     PORT_FILE_DATA  //header + $8
        sta     sidMemStart + 1
        lda     PORT_FILE_DATA  //header + $9
        sta     sidMemStart
        //initAddress
        lda     PORT_FILE_DATA  //header + $a
        sta     sidInitAddr + 1
        lda     PORT_FILE_DATA  //header + $b
        sta     sidInitAddr
        //playAddress
        lda     PORT_FILE_DATA  //header + $c
        sta     sidPlayAddr + 1
        lda     PORT_FILE_DATA  //header + $d
        sta     sidPlayAddr
        //songs
        lda     PORT_FILE_DATA  //header + $e
        lda     PORT_FILE_DATA  //header + $f
        //StartSong
        lda     PORT_FILE_DATA  //header + $10
        lda     PORT_FILE_DATA  //header + $11
        sta     sidSubSong          
        //send $12 to $7C to /dev/null
        ldx     #($7C - $12)
        {
            lda     PORT_FILE_DATA
            dex        
            bne     @loop
        }  
        //memStart
        lda     PORT_FILE_DATA
        sta     sidMemStart
        lda     PORT_FILE_DATA
        sta     sidMemStart + 1          
        //load the song data to memory
        {
            //read byte from file
            ldx     PORT_FILE_DATA
            //check status on read, end if no more bytes to read
            lda     PORT_FILE_STATUS
            cmp     #FILE_STATUS.NOK
            beq     @continue        
            //store data
            stx     sidMemStart: $1234
            //inc destination
            inc     sidMemStart
            bne     @loop
            inc     sidMemStart + 1
            jmp     @loop        
        }
        //close file
        lda     #1
        sta     PORT_FILE_CMD             
        rts        
    }

    subroutine sidInit
    {
        //init sub-song
        lda     sidSubSong: #0
        jsr     sidInitAddr: $1234
        //set master volume to $f
        lda     PORT_SID_18
        ora     #$0f
        sta     PORT_SID_18
        //set max slot time for self
        lda     #OS.THREAD_SELF_GET
        jsr     osCall
        lda     #255
        sta     osCallData[1]
        lda     #OS.THREAD_SET_SLOT_TIME
        jsr     osCall
        rts
    }

    subroutine sidPlay
    {
        {
            //wait for new frame
            lda     PORT_FRAME_COUNTER_LO
            {
                cmp     PORT_FRAME_COUNTER_LO
                bne     @continue
                sta     PORT_IRQ_TIMER_TRIG     //yield
                jmp     @loop                
            }

            //set the SID register values from last time
            ldx     #0
            {
                lda     $d400,x
                sta     PORT_SID_00,x
                inx
                cpx     #$1d
                bne     @loop
            }
            //call sid-play
            jsr     sidPlayAddr: $1234
            //loop            
            jmp     @loop
        }
    }

    subroutine sidStop
    {
        //set master volume to 0
        lda     PORT_SID_18
        and     #$f0
        sta     PORT_SID_18
        //set flag to disable play
        //TODO: create flag
        //return
        rts
    }
}
