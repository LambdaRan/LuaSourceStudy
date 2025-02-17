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
关于realloc函数：
void * frealloc(void *ud, void *ptr, size_t osize, size_t nsize);
（`osize'是旧大小，`nsize'是新大小）

* frealloc(ud, NULL, x, s)创建一个大小为`s'的新块（无论`x'是多少）。

* frealloc(ud, p, x, 0)释放块`p'（在这种特殊情况下，frealloc必须返回NULL）；
特别是，frealloc(ud, NULL, 0, 0)不执行任何操作（这等同于ANSI C中的free(NULL)）。

如果无法创建或重新分配区域，frealloc将返回NULL（任何大小相等或更小的重新分配都不会失败！）
*/

#define MINSIZEARRAY	4

// 管理可变长数组
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
参数说明
lua_State *L：当前 Lua 状态的指针。
void *ptr：指向之前分配的内存块的指针。如果为 NULL，则表示分配新内存。
size_t oldsize：之前分配的内存块的大小（以字节为单位）。如果 ptr 为 NULL，则 oldsize 应为 0。
size_t newsize：新内存块的大小（以字节为单位）。如果 newsize 为 0，则表示释放内存。

返回值
返回指向新分配内存块的指针。如果 newsize 为 0，则返回 NULL。

使用说明：
内存分配
如果 ptr 为 NULL 且 newsize 大于 0，则分配一块新的内存。
如： #define luaM_malloc(L,s)	luaM_realloc_(L, NULL, 0, (s))

内存重新分配
如果 ptr 不为 NULL 且 newsize 大于 0，则重新分配内存块的大小。
如果 newsize 大于 oldsize，可能会扩展内存块；如果 newsize 小于 oldsize，可能会缩小内存块。

内存释放
如果 newsize 为 0，则释放 ptr 指向的内存块。
如：#define luaM_freemem(L, b, s)	luaM_realloc_(L, (b), (s), 0)

参考：
默认g->frealloc是函数 static void *l_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {}
nsize为0的时候释放内存
其他情况 扩展内存、申请内存，裁剪
*/
