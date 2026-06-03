Based on the following tests, DMC DMA adds 4 cycles normally, 3 if it lands on a CPU write, 2 if it lands on the $4014 write or during OAM DMA, 1 if on the next-to-next-to-last DMA cycle, 3 if on the last DMA cycle. The test ROMs here print the below outputs, and verify that they match what's expected.

sprdma_and_dmc_dma.zip

They also verify that the bytes copied to OAM match what is expected, so DMC DMA isn't corrupting the data. Further, the DMC sample playing during the test is all $55 bytes, so if the DMC DMA read were corrupted, it'd be audible. I recorded output and don't see any corruption.

This test has DMC DMA occur at each cycle in a piece of code, and prints how many cycles the code took, including any extra cycles the DMA added. For example, this code generates the output after it:
Code: Select all

sta $100    ; 4
lda $100    ; 4
sta $100    ; 4
sta $100    ; 4
        
T+ Clocks (decimal)
00 20
01 20
02 20
03 19
04 20
05 20
06 20
07 20
08 20
09 20
0A 20
0B 19
0C 20
0D 20
0E 20
0F 19
The code should take 16 cycles, but DMA adds four. However, when it lands on the write cycles of the three STA instructions, it only takes three. You can clearly see the pattern of the STA-LDA-STA-STA in the result, confirming that it's really measuring something useful.

Now, the code that tests sprite DMA:
Code: Select all

lda #$07    ; 2
sta $4014   ; 4 + 513/514
sta $100    ; 4

T+ Clocks (decimal)
00 527      +4  LDA #$07    ; 2
01 528      +4
02 527      +4  STA $4014   ; 4 + 513/514
03 528      +4
04 527      +4
05 526      +2
06 525      +2
07 526      +2
08 525      +2
09 526      +2
0A 525      +2
0B 526      +2
0C 525      +2
0D 526      +2
0E 525      +2
0F 526      +2
...
200 525     +2
201 526     +2
202 525     +2
203 526     +2
204 524     +1  DMA next-to-next-to-last cycle
205 525     +1  DMA next-to-next-to-last cycle
206 526     +3  DMA last cycle
207 527     +3  DMA last cycle
208 527     +4  STA $100 second cycle
209 528     +4  STA $100 second cycle
20A 526     +3  STA $100 fourth cycle
20B 527     +3  STA $100 fourth cycle
I've manually listed the number of DMA cycles added (clocks-523/524), and what instruction is executing. The main snag is that sprite DMA takes 513 OR 514 cycles, depending on whether it's started on an even or odd 2A03 cycle. I'm assuming this is very similar to $4017 writes being delayed a cycle if on an odd 2A03 cycle.

The way this test works, the test code begins on even/odd 2A03 cycles based on the time it has arranged the DMC DMA to occur. This complicates things. At the end of OAM DMA and after, it means that DMC DMA is only hitting every other cycle of the test code. You can see this in the STA $100 after OAM DMA, where DMC DMA takes three cycles for two different times. This is because both times it's landing on the fourth cycle of STA $100 (I tried other instruction sequences to be sure of this, and it checks out).

Maybe someone with a logic analyzer can see what's really going on. The above is about as much as you're going to get with a CPU test alone. :)