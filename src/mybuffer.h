#ifndef mybuffer_h
#define mybuffer_h

/* Written by Gemini AI, 7.12.2, 1 to 2 % faster with strings smaller than 1,024 characters,
   7 to 8 % faster with larger strings */

/* YOU MUST CALL my_cleanup() if you leave a buffer operation early before returning the buffer contents
   with my_pushresult(), otherwise youwe will have memory leaks !!! */

#include "agena.h"
#include "agnxlib.h"

#include "mybuffer.h"

typedef struct {
  lua_State *L;
  char *ptr;         /* Pointer to dynamically allocated buffer */
  size_t capacity;   /* Max allocated space */
  size_t size;       /* Current length of string data */
  char initb[LUAL_BUFFERSIZE];  /* Small fast stack buffer to avoid initial malloc */
} MyBuffer;

LUALIB_API void my_buffinit (lua_State *L, MyBuffer *B);
LUALIB_API void my_addlstring (MyBuffer *B, const char *s, size_t l);
LUALIB_API void my_pushresult (MyBuffer *B);
LUAI_FUNC  void my_buffgrow (MyBuffer *B, size_t extra);  /* Do NOT call this function directly in client code */
LUALIB_API void my_addchar_heavy (MyBuffer *B, char c);

#define my_addchar(B, c) \
  ((void)((B)->size < (B)->capacity ? 0 : (my_buffgrow(B, 1), 0)), \
   (B)->ptr[(B)->size++] = (c))

#define my_addchar_unsafe(B, c) \
   ((B)->ptr[(B)->size++] = (c))

#define my_cleanup(B) \
  ((void)((B)->ptr != (B)->initb ? (free((B)->ptr), 0) : 0))

#endif
