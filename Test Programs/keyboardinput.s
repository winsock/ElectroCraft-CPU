main:
mov ecx, 0
mov ebx [0x1010000]
mul ebx [0x1010004]
input:
inp eax, 0x122
cmp eax, 0
je input
mov edx, [0x1010008]
add edx, ecx
mov [edx], eax
cmp ecx, ebx
je main
add ecx, 1
jmp input