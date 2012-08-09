mov ecx, 0
mov edx, 0
mov emc, [0x1010008]
mainLoop:
inp eax, 0x122 ; Read the next key
out 0x121, 0 ; Clear the buffer of any extra keys
cmp eax, 0xA
je enter
push ecx
mul ecx, edx
push emc
add emc, ecx
mov [emc], [ebx]
pop emc
pop ecx
add ecx, 1
jmp end
enter:
add edx, 1
jmp end
end:
jmp mainLoop