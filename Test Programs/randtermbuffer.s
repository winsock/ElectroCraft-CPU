; Puts random text on the terminal buffer
mov edx, [0x1010008]
resetLoop:
mov ecx, [0x1010000]
mul ecx, [0x1010004]
push edx
main:
add edx, 0x1
mov eax, 33
randi eax, 127
mov [edx], eax
mov [edx], 0x4C
loop main
pop edx
jmp resetLoop