set pagination off
set confirm off
set remotetimeout 30
file build/Debug/finalboardTX.elf
target remote 127.0.0.1:61234
monitor reset
monitor halt
load
compare-sections
monitor reset
monitor halt
detach
quit
