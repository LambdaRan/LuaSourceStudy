/*
** $Id: lmem.c,v 1.84 2012/05/23 15:41:53 roberto Exp $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/


#include <stddef.h>

#define lmem_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"



/*
** About the realloc function:
** void * frealloc (void *ud, void *ptr, size_t osize, size_t nsize);
** (`osize' is the old size, `nsize' is the new size)
**
** * frealloc(ud, NULL, x, s) creates a new block of size `s' (no
** matter 'x').
**
** * frealloc(ud, p, x, 0) frees the block `p'
** (in this specific case, frealloc must return NULL);
** particularly, frealloc(ud, NULL, 0, 0) does nothing
** (which is equivalent to free(NULL) in ANSI C)
**
** frealloc returns NULL if it cannot create or reallocate the area
** (any reallocation to an equal or smaller size cannot fail!)
*/

/*
����realloc������
void * frealloc(void *ud, void *ptr, size_t osize, size_t nsize);
��`osize'�Ǿɴ�С��`nsize'���´�С��

* frealloc(ud, NULL, x, s)����һ����СΪ`s'���¿飨����`x'�Ƕ��٣���

* frealloc(ud, p, x, 0)�ͷſ�`p'����������������£�frealloc���뷵��NULL����
�ر��ǣ�frealloc(ud, NULL, 0, 0)��ִ���κβ��������ͬ��ANSI C�е�free(NULL)����

����޷����������·�������frealloc������NULL���κδ�С��Ȼ��С�����·��䶼����ʧ�ܣ���
*/

#define MINSIZEARRAY	4

// ����ɱ䳤����
void *luaM_growaux_ (lua_State *L, void *block, int *size, size_t size_elems,
                     int limit, const char *what) {
  void *newblock;
  int newsize;
  if (*size >= limit/2) {  /* cannot double it? */
    if (*size >= limit)  /* cannot grow even a little? */
      luaG_runerror(L, "too many %s (limit is %d)", what, limit);
    newsize = limit;  /* still have at least one free place */
  }
  else {
    newsize = (*size)*2;
    if (newsize < MINSIZEARRAY)
      newsize = MINSIZEARRAY;  /* minimum size */
  }
  newblock = luaM_reallocv(L, block, *size, newsize, size_elems);
  *size = newsize;  /* update only when everything else is OK */
  return newblock;
}


l_noret luaM_toobig (lua_State *L) {
  luaG_runerror(L, "memory allocation error: block too big");
}



/*
** generic allocation routine.
*/
void *luaM_realloc_ (lua_State *L, void *block, size_t osize, size_t nsize) {
  void *newblock;
  global_State *g = G(L);
  size_t realosize = (block) ? osize : 0;
  lua_assert((realosize == 0) == (block == NULL));
#if defined(HARDMEMTESTS)
  if (nsize > realosize && g->gcrunning)
    luaC_fullgc(L, 1);  /* force a GC whenever possible */
#endif
  newblock = (*g->frealloc)(g->ud, block, osize, nsize);
  if (newblock == NULL && nsize > 0) {
    api_check(L, nsize > realosize,
                 "realloc cannot fail when shrinking a block");
    if (g->gcrunning) {
      luaC_fullgc(L, 1);  /* try to free some memory... */
      newblock = (*g->frealloc)(g->ud, block, osize, nsize);  /* try again */
    }
    if (newblock == NULL)
      luaD_throw(L, LUA_ERRMEM);
  }
  lua_assert((nsize == 0) == (newblock == NULL));
  g->GCdebt = (g->GCdebt + nsize) - realosize;
  return newblock;
}

/*
����˵��
lua_State *L����ǰ Lua ״̬��ָ�롣
void *ptr��ָ��֮ǰ������ڴ���ָ�롣���Ϊ NULL�����ʾ�������ڴ档
size_t oldsize��֮ǰ������ڴ��Ĵ�С�����ֽ�Ϊ��λ������� ptr Ϊ NULL���� oldsize ӦΪ 0��
size_t newsize�����ڴ��Ĵ�С�����ֽ�Ϊ��λ������� newsize Ϊ 0�����ʾ�ͷ��ڴ档

����ֵ
����ָ���·����ڴ���ָ�롣��� newsize Ϊ 0���򷵻� NULL��

ʹ��˵����
�ڴ����
��� ptr Ϊ NULL �� newsize ���� 0�������һ���µ��ڴ档
�磺 #define luaM_malloc(L,s)	luaM_realloc_(L, NULL, 0, (s))

�ڴ����·���
��� ptr ��Ϊ NULL �� newsize ���� 0�������·����ڴ��Ĵ�С��
��� newsize ���� oldsize�����ܻ���չ�ڴ�飻��� newsize С�� oldsize�����ܻ���С�ڴ�顣

�ڴ��ͷ�
��� newsize Ϊ 0�����ͷ� ptr ָ����ڴ�顣
�磺#define luaM_freemem(L, b, s)	luaM_realloc_(L, (b), (s), 0)

�ο���
Ĭ��g->frealloc�Ǻ��� static void *l_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {}
nsizeΪ0��ʱ���ͷ��ڴ�
������� ��չ�ڴ桢�����ڴ棬�ü�
*/
