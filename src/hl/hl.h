#ifndef _HL_H_
#define _HL_H_

#include <stdio.h>

char *C_HL_extensions[] = { ".c", ".h", ".cpp", ".hpp", ".cc", NULL };
char *ASM_HL_extensions[] = { ".s", ".S", ".asm", NULL };

char *C_HL_keywords[] = {
  "switch", "if", "while", "for", "break", "continue", "return", "else",
  "struct", "union", "typedef", "static", "enum", "class", "case",
  "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
  "void|", "#include|", "#define", "sizeof", "extern", "const|", "volatile",
  "register", "auto", "do", "default", "goto", "restrict", "inline", "bool|",
  "#ifndef", "#endif", NULL
};

char *ASM_HL_keywords[] = {
  "mov|", "add|", "sub|", "mul|", "div|", "jmp|", "jne|", "je|", "jg|", "jge|", "jl|", 
  "jle|", "and|", "or|", "xor|", "not|", "shl|", "shr|", "neg|", "lea|", "pop|", "push|", "int|",
  "ret|", "call|", "cmp|", "test|", "inc|", "dec|", "eax", "ebx", "ecx", "edx",
  ".data", ".text", "global|", "section|", ".bss", "equ", ".word", ".long" 
  "esi", "edi", "esp", "ebp", "cs", "ds", "es", "fs", "gs", "ss", "db", NULL
};

#endif // _HL_H_
