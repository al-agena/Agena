#ifndef lstrlib_h
#define lstrlib_h

const char *lmemfind (const char *s1, size_t l1, const char *s2, size_t l2);

/* ptrdiff_t posrelat (ptrdiff_t pos, size_t len); */

typedef struct MatchState {
  const char *src_init;  /* init of source string */
  const char *src_end;  /* end (`\0') of source string */
  lua_State *L;
  int level;  /* total number of captures (finished or unfinished) */
  struct {
    const char *init;
    ptrdiff_t len;
  } capture[LUA_MAXCAPTURES];
} MatchState;

#define SPECIALS   "^$*+?.([%-"
void INLINE initMatchState (lua_State *L, MatchState *ms, int level, const char *s, size_t ls);
int matchbracketclass (int c, const char *p, const char *ec);
int singlematch (int c, const char *p, const char *ep);
const char *match (MatchState *ms, const char *s, const char *p, int mode);
const char *matchbalance (MatchState *ms, const char *s, const char *p);
const char *max_expand (MatchState *ms, const char *s, const char *p, const char *ep, int mode);
const char *min_expand (MatchState *ms, const char *s, const char *p, const char *ep, int mode);
const char *start_capture (MatchState *ms, const char *s, const char *p, int what, int mode);
const char *end_capture (MatchState *ms, const char *s, const char *p, int mode);
const char *match_capture (MatchState *ms, const char *s, int l);
const char *classend (MatchState *ms, const char *p);
void aux_splitcheckoptions (lua_State *L, int idx,
  int *nargs, int fromnarg, int *init, int *convert, int *match, int *bailout, char **unwrap, char **delim,
  int *header, int *ignorefnidx, int *skipfaultylines, const char *procname);

/*
** Some sizes are better limited to fit in 'int', but must also fit in
** 'size_t'. (We assume that 'lua_Integer' cannot be smaller than 'int'.)
*/
#define MAXSIZE  \
	(sizeof(size_t) < sizeof(int) ? MAX_SIZET : (size_t)(INT_MAX))

#endif
