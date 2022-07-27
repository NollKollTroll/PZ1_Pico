/*
Written in 2022 by Adam Klotblixt (adam.klotblixt@gmail.com)

To the extent possible under law, the author have dedicated all
copyright and related and neighboring rights to this software to the
public domain worldwide.
This software is distributed without any warranty.

You should have received a copy of the CC0 Public Domain Dedication
along with this software. If not, see
<http://creativecommons.org/publicdomain/zero/1.0/>.
*/

1000 REM print info about active threads
1010 PRINT "Thread","Slot","Blocks"
1020 PRINT "nr","time","used"
1030 FOR I=0 TO 22
1040 IF 1 <> TSTATER(I) THEN GOTO 1110
1050 PRINT I,TSLOTR(I),TBLKR(I);
1060 FOR J=228 TO 230
1070 IF 254 = PEEK(J) THEN GOTO 1100
1080 PRINT PEEK(J);
1090 NEXT J
1100 PRINT
1110 NEXT I
1120 GOTO1000

