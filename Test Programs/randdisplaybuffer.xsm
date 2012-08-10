; A Simple program that changes the display buffer randomly
.code
rand:
mov eax, 0 ; move the lower limit of the random function to eax
randi eax, 2000000 ; call the random function with the range of 0-2000000 and store the result in eax
mov ecx, [0x8004] ; Get the value at 0x8004 The value there is the size of the display buffer
mov edx, [0x800C] ; Get the display buffer address
add ecx, edx ; Add the lower limit of the diplay buffer
randi edx, ecx ; get a random address between 0x8004 and the value of the size of the display buffer
mov [edx], eax ; set the pixel at the random address
call sleep ; Sleep the program to prevent eating up CPU cycles
jmp rand ; And jump back to the begining and repeat
sleep:
mov cx, 100 ; Loop 100 times
sleeploop:\n
nop ; No instruction
loop sleeploop ; Loop while cx > 0
ret ; return to the caller\n;