set pagination off
set confirm off
set remotetimeout 15
file build/Debug/finalboardTX.elf
target remote 127.0.0.1:61234
printf "BASELINE BEFORE REPAIR\n"
p/x $pc
bt 10
p/x *(unsigned int*)0xE000ED28
p/x *(unsigned int*)0xE000ED2C
p/x *(unsigned int*)0xE000ED34
p uwTick
p SystemCoreClock
p uxCurrentNumberOfTasks
p/x *(unsigned int*)0x40016818
p/x *(unsigned int*)0x400168AC
p/x *(unsigned int*)0xA0000154
p/x *(unsigned int*)0x40022014
p/x *(unsigned int*)0x40022814
dump binary memory debug_evidence/baseline_flash.bin 0x08000000 0x08100000
detach
quit
