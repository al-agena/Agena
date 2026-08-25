/* Character Buffer;

   Call charbuf_finish if you want to use the buffered string but do not know its size, e.g. when using lua_pushstring, after the last
   append, to include a final and terminating '\0'. */

#ifndef memfile_h
#define memfile_h

#include <stdlib.h>
#include "agena.h"


void memfile_issueiniterror (lua_State *L, size_t wrongalignedbytes, size_t wrongnewsize);

#endif

