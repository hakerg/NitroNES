dma_2007_read
; DMC DMA during $2007 read causes 2-3 extra $2007
; reads before real read.
;
; Number of extra reads depends in CPU-PPU
; synchronization at reset.
;
; Output:
;11 22 
;11 22 
;33 44 or 44 55
;11 22 
;11 22 
;159A7A8F or 5E3DF9C4


dma_2007_write
; DMC DMA during $2007 write has no effect.
;
; Output:
;22 11 22 AA 44 55 66 77 
;22 11 22 AA 44 55 66 77 
;22 11 22 AA 44 55 66 77 
;22 11 22 AA 44 55 66 77 
;22 11 22 AA 44 55 66 77


dma_4016_read
; DMC DMA during $4016 read causes extra $4016
; read.
;
; Output:
;08 08 07 08 08


double_2007_read
; Double read of $2007 sometimes ignores extra
; read, and puts odd things into buffer.
;
; Output (depends on CPU-PPU synchronization):
;22 33 44 55 66 
;22 44 55 66 77 or
;22 33 44 55 66 or
;02 44 55 66 77 or
;32 44 55 66 77 or
;85CFD627 or F018C287 or 440EF923 or E52F41A5


read_write_2007
; Read of $2007 just before write behaves normally.
;
; Output:
;33 11 22 33 09 55 66 77 
;33 11 22 33 09 55 66 77 