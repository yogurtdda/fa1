set pagination off
set confirm off
set remotetimeout 15
file build/Debug/finalboardTX.elf
target remote 127.0.0.1:61234
monitor halt
p/x $pc
bt 4
p stream_diag
p pwm_diag
p display_diag
p touch_diag
p frequency_ui_diag
p/x hadc3.ErrorCode
p/x DMA2_Stream0->CR
p/x DMA2_Stream0->PAR
p/x DMA2_Stream0->M0AR
p/x TIM8->CR2
p/x TIM8->DIER
p/x *(unsigned int*)0xE000ED28
p/x *(unsigned int*)0xE000ED2C
p xFreeBytesRemaining
p/x &block_ring
set $slot = (stream_diag.producer - 1) % 8
p block_ring[$slot].sequence
p block_ring[$slot].pwm_hz
dump binary memory debug_evidence/packed_block.bin &block_ring[$slot].records[0] &block_ring[$slot].records[1024]
detach
quit
