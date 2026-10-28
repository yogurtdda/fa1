set pagination off
set confirm off
set remotetimeout 15
file build/Debug/finalboardTX.elf
target remote 127.0.0.1:61234
p/x $pc
bt 4
p display_diag
p board_sdram_status
p uxCurrentNumberOfTasks
p/x *(unsigned int*)0xE000ED28
p/x *(unsigned int*)0xE000ED2C
p/x *(unsigned int*)0x40016838
p/x *(unsigned int*)0x400168AC
p/x *(unsigned int*)0xA0000154
set $front = *(unsigned int*)0x400168AC
if display_diag.frames > 1 && ($front == 0xC0000000 || $front == 0xC0040000)
  dump binary memory debug_evidence/lcd_rgb565.bin $front ($front + 261120)
end
detach
quit
