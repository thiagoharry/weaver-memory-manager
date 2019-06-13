/*1:*/
#line 175 "./weaver-memory-manager.tex"

#ifndef WEAVER_MEMORY_MANAGER
#define WEAVER_MEMORY_MANAGER
#ifdef __cplusplus
extern"C"{
#endif
/*2:*/
#line 221 "./weaver-memory-manager.tex"

#include <stdlib.h>  
void*Wcreate_arena(size_t size);
/*:2*//*3:*/
#line 231 "./weaver-memory-manager.tex"

#include <stdbool.h>  
bool Wdestroy_arena(void*);
/*:3*//*4:*/
#line 246 "./weaver-memory-manager.tex"

void*Walloc(void*arena,int alignment,int right,size_t size);
/*:4*//*5:*/
#line 259 "./weaver-memory-manager.tex"

bool Wbreakpoint(void*arena,int regiao);
/*:5*//*6:*/
#line 270 "./weaver-memory-manager.tex"

void Wtrash(void*arena,int regiao);
/*:6*/
#line 181 "./weaver-memory-manager.tex"

#ifdef __cplusplus
}
#endif
#endif
/*:1*/
