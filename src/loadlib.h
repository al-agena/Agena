#ifndef loadlib_h
#define loadlib_h

const char *fastloader_C (lua_State *L, const char *filename, const char *packagename, int *success, int printerror);
const char *pushnexttemplate (lua_State *L, const char *path);

#endif
