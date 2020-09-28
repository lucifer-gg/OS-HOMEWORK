section .data
color_blue:    db 1Bh, '[34;1m', 0 ;蓝色
.len           equ $ - color_blue
color_red:     db 1Bh, '[31;1m', 0 ;红色
.len           equ $ - color_red
defaultColor:  db 1bh,'[0m'        ;默认颜色
.len           equ $ - defaultColor

section .text
global my_print
;global turnBlue
;global turnDefault

my_print:
  mov eax, [esp + 12]
  cmp eax, 0
  jz print
  cmp eax, 1
  jz turnBlue
  cmp eax, 2
  jz turnRed

turnRed:
  mov eax, 4
  mov ebx, 1
  mov ecx, color_red
  mov edx, color_red.len
  int 80h
  jmp print
  
turnBlue:
  mov eax, 4
  mov ebx, 1
  mov ecx, color_blue
  mov edx, color_blue.len
  int 80h
  jmp print
  
print:
  mov eax, 4
  mov ebx, 1
  mov ecx, [esp + 4]
  mov edx, [esp + 8]
  int 80h
 
turnDefault:
  mov eax, 4
  mov ebx, 1
  mov ecx, defaultColor
  mov edx, defaultColor.len
  int 80h
  ret
