#!/bin/bash

assemble ()
{
    echo
    echo \#### $1
    #remove old files
    rm $1.bin $1.prg $1.h $1_hex_dump.txt $1_sym.txt
    if [ $2 ] ; then
        rm $1_srec.txt
    fi
    #assemble -> bin/hex_dump/sym
    jasm -p 65c02 -pi -v2 -dh $1_hex_dump.txt -ds $1_sym.txt $1.asm $1.bin
    #create bin with start-addr header
    jasm -p 65c02 -pi -v0 -hla $1.asm $1.prg
    #convert bin to h-file
    xxd -i $1.bin $1.h
    #convert bin to srec:
    if [ $2 ] ; then
        if [ $2 -eq -65535 ] ; then
            let OFFSET=65536-$(stat --format=%s $1.bin)
        elif [ $2 -lt 0 ] ; then
            let OFFSET=65536+$2-$(stat --format=%s $1.bin)
        else
            let OFFSET=$2
        fi
        echo srec offset: \$$(printf %X $OFFSET)
        srec_cat $1.bin -Binary -offset $OFFSET -Output $1_srec.txt -Motorola -Output_Block_Alignment -Execution_Start_Address $OFFSET -HEAder $1.bin
    fi
}

echo \#### Starting assembly
cd 6502-src
assemble toppage
assemble ehbasic
echo
echo \#### Assembly done
