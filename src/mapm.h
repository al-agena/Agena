#ifndef mapm_h
#define mapm_h

#define MAPMXTYPE		"xnumber"
#define MAPMCTYPE		"cnumber"

#define ismapm(L,n)   (luaL_isudata(L, n, MAPMXTYPE))
#define iscmapm(L,n)  (luaL_isudata(L, n, MAPMCTYPE))

#endif
