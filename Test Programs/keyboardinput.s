.code
mov ecx, [0x1010008]
main:
push ecx
call processKey
pop emc
cmp emc, 1
je remove
cmp emc, 2
je doEnter
add ecx, 1
jmp main
remove:
sub ecx, 1
mov [ecx], 0
jmp main
doEnter:
add ecx, [0x1010000]
jmp main
        
processKey:
push ebp
mov ebp, esp
sub esp, 4 ; Allocate 4 bytes to store result
call getKey
pop ebx
cmp ebx, 8
je backspace
cmp ebx, 0xA
je enter
cmp ebx, 0xD
je enter
push ebx
push [ebp + 8]
call displayKey
add esp, 8
mov [ebp + 8], 0
jmp end
enter:
mov [ebp + 8] , 2
jmp end
backspace:
mov [ebp + 8], 1
end:
mov esp, ebp
pop ebp
ret
        
getKey:
push ebp
mov ebp, esp
inp [ebp + 8], 0x122
mov esp, ebp
pop ebp
ret
        
displayKey:
push ebp
mov ebp, esp
mov [ebp + 12], [ebp + 12]
mov esp, ebp
pop ebp
ret