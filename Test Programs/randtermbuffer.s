; Puts random text on the terminal buffer
mov ebx, [0x1008004]
mov edx, [0x1008008]
charLoop:
push edx
mov ecx, [0x1008000]
counter:
add edx, ecx
mov [edx], 0xC4
loop counter
pop edx
jmp charLoop