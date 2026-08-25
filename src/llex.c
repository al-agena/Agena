/*
** $Id: llex.c,v 2.20 2006/03/09 18:14:31 roberto Exp $
** Lexical Analyzer
** See Copyright Notice in agena.h
*/


#include <ctype.h>
#include <locale.h>
#include <string.h>

#define llex_c
#define LUA_CORE

#include "agena.h"

#include "ldo.h"
#include "llex.h"
#include "lobject.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lzio.h"
#include "lucase.def"

#define next(ls) (ls->current = zgetc(ls->z))

#define currIsNewline(ls)   (ls->current == '\n' || ls->current == '\r')

/* ORDER RESERVED */
const char *const luaX_tokens [] = {
    "abs", "alias", "and", "antilog2", "antilog10", "arccos", "arcsec", "arcsin", "arctan", "as", "assigned", "atendof",  /* 12 */
    "bea", "begin", "bottom", "break", "by", "bye",  /* 18 */
    "case", "catch", "cell", "cis", "clear", "cls", "conjugate", "constant", "cos", "cosh", "cosxx", "create", "cube",  /* 31 */
    "dec", "def", "define", "delete", "dict", "div", "do", "downto", "duplicate",
    "elif", "else", "empty", "end", "entier", "enum", "epocs", "esac", "esle", "even", "exchange", "exp",
    "fail", "false", "feature", "fi", "filled", "finite", "flip", "for", "foreach", "frac", "fractional", "from", "global",
    "if", "imag", "import", "in", "inc", "infinite", "insert", "int", "intdiv", "integral",
    "intersect", "into", "invsqrt", "is", "keys", "left", "ln", "lngamma", "local", "log", "minus",
    "mod", "mul", "muladd", "mulup", "nan", "nand", "nargs", "negate", "next", "nonzero", "nor", "not", "nothing", "notin", "null",
    "od", "odd", "of", "onsuccess", "or", "pop", "post", "pre", "proc", "procname", "pushd", "qmdev", "qsumup",
    "real", "recip", "redo", "reg", "relaunch", "reminisce", "restart", "return", "right", "roll", "rotate",
    "scope", "seq", "sign", "signum", "sin", "sinc", "sinh", "size", "skip", "split", "sqrt", "square", "squareadd",
    "store", "subset", "sumup", "switchd", "symmod", "tan", "tanh", "then", "to", "top", "true", "try", "type", "typeof",
    "unassigned", "unity", "union", "unless", "until", "when", "while", "with", "xnor", "xor", "xsubset", "yrt", "zero",
    "boolean", "number", "complex", "string", "procedure", "userdata", "lightuserdata", "thread", "table",
    "sequence", "register", "pair", "set", "integer", "posint", "nonnegint", "nonzeroint", "positive", "negative", "nonnegative", "listing",
    "basic", "anything",
    /* let 'anything' be the last word before puntuations start; all punctuation marks must be listed from here: */
    ":=", "~", "&", "?", "-?", "=", ">", "<", ">=", "<=", "<>", "==", "~=", "~<>", "**", "@", "$", "$$", "$$$",
    "::", "-:", "&&", "~~", "||", "^^", "<<", ">>", "<<<", ">>>",
    "*%", "/%", "+%", "-%", "%%", "->", "<:", ":>",
    "!", "!!", "..", "@@", "++", "--", "+++", "---", "//", "\\\\", "(/", "\\)", "|-", "~|",
    "+:=", "-:=", "*:=", "@:=", "/:=", "\\:=", "%:=", "&:=",
    "&&:=", "||:=", "^^:=", "<<:=", ">>:=", "<<<:=", ">>>:=",
    "&+", "&-", "&*", "%/", "&\\",
    "<number>", "<name>", "<string>", "<eof>"
};

#define save_and_next(ls) (save(ls, ls->current), next(ls))


static void save (LexState *ls, int c) {
  Mbuffer *b = ls->buff;
  if (b->n + 1 > b->buffsize) {
    size_t newsize;
    if (b->buffsize >= MAX_SIZET/2)
      luaX_lexerror(ls, "lexical element too long", 0);
    newsize = b->buffsize * 2;
    luaZ_resizebuffer(ls->L, b, newsize);
  }
  b->buffer[b->n++] = cast(char, c);
}


void luaX_init (lua_State *L) {
  int i;
  for (i=0; i < NUM_RESERVED; i++) {
    TString *ts = luaS_new(L, luaX_tokens[i]);
    luaS_fix(ts);  /* reserved words are never collected */
    lua_assert(tools_strlen(luaX_tokens[i]) + 1 <= TOKEN_LEN);  /* 2.17.8 tweak */
    ts->tsv.reserved = cast_byte(i + 1);  /* reserved word */
  }
}


#define MAXSRC          80

static unsigned char comment_oldstyle = 0;  /* counter for nested old-style comments ###/// */
static unsigned char comment_newstyle = 0;  /* counter for nested old-style comments \* */

const char *luaX_token2str (LexState *ls, int token) {
  if (token < FIRST_RESERVED) {
    lua_assert(token == cast(unsigned char, token));
    return (iscntrl(token)) ? luaO_pushfstring(ls->L, "char(%d)", token) :
                              luaO_pushfstring(ls->L, "%c", token);
  }
  else
    return luaX_tokens[token - FIRST_RESERVED];
}


static const char *txtToken (LexState *ls, int token) {
  switch (token) {
    case TK_NAME:
    case TK_STRING:
    case TK_NUMBER:
      save(ls, '\0');
      return luaZ_buffer(ls->buff);
    default:
      return luaX_token2str(ls, token);
  }
}


void luaX_lexerror (LexState *ls, const char *msg, int token) {
  char buff[MAXSRC];
  luaO_chunkid(buff, getstr(ls->source), MAXSRC);
  /* msg = luaO_pushfstring(ls->L, "%s:%d: %s", buff, ls->linenumber, msg); */
  msg = tools_strneq(buff, "stdin") ?
     luaO_pushfstring(ls->L, "Error in %s at line %d:\n   %s", buff, ls->linenumber, msg) :
     luaO_pushfstring(ls->L, "Error at line %d: %s", ls->linenumber, msg);
  if (token)
    luaO_pushfstring(ls->L, "%s near " LUA_QS, msg, txtToken(ls, token));
  luaD_throw(ls->L, LUA_ERRSYNTAX);
}


void luaX_syntaxerror (LexState *ls, const char *msg) {
  luaX_lexerror(ls, msg, ls->t.token);
}


TString *luaX_newstring (LexState *ls, const char *str, size_t l) {
  lua_State *L = ls->L;
  TString *ts = luaS_newlstr(L, str, l);
  TValue *o = luaH_setstr(L, ls->fs->h, ts);  /* entry for `str' */
  if (ttisnil(o)) {
    setbvalue(o, 1);  /* make sure `str' will not be collected */
    luaC_checkGC(L);  /* Lua 5.1.4 patch 6 */
  }
  return ts;
}


/*
** increment line number and skips newline sequence (any of
** \n, \r, \n\r, or \r\n)
*/
static void inclinenumber (LexState *ls) {
  int old = ls->current;
  lua_assert(currIsNewline(ls));
  next(ls);  /* skip `\n' or `\r' */
  if (currIsNewline(ls) && ls->current != old)
    next(ls);  /* skip `\n\r' or `\r\n' */
  if (++ls->linenumber >= MAX_INT)
    luaX_syntaxerror(ls, "chunk has too many lines");
}


void luaX_setinput (lua_State *L, LexState *ls, ZIO *z, TString *source) {
  ls->decpoint = '.';
  ls->L = L;
  ls->lookahead.token = TK_EOS;  /* no look-ahead token */
  ls->z = z;
  ls->fs = NULL;
  ls->linenumber = 1;
  ls->lastline = 1;
  ls->source = source;
  luaZ_resizebuffer(ls->L, ls->buff, LUA_MINBUFFER);  /* initialize buffer */
  next(ls);  /* read first char */
}

/*
** =======================================================
** LEXICAL ANALYZER
** =======================================================
*/



static int check_next (LexState *ls, const char *set) {
  if (!strchr(set, ls->current))
    return 0;
  save_and_next(ls);
  return 1;
}


static void buffreplace (LexState *ls, char from, char to) {
  size_t n = luaZ_bufflen(ls->buff);
  char *p = luaZ_buffer(ls->buff);
  while (n--)
    if (p[n] == from) p[n] = to;
}


static void trydecpoint (LexState *ls, SemInfo *seminfo, int *overflow) {
  /* format error: try to update decimal point separator */
  struct lconv *cv = localeconv();
  char old = ls->decpoint;
  ls->decpoint = (cv ? cv->decimal_point[0] : '.');
  buffreplace(ls, old, ls->decpoint);  /* try updated decimal separator */
  if (!luaO_str2d(luaZ_buffer(ls->buff), &seminfo->r, overflow)) {
    /* format error with correct decimal point: no more options */
    buffreplace(ls, ls->decpoint, '.');  /* undo change (for error message) */
    luaX_lexerror(ls, "malformed number", TK_NUMBER);
  }
}


/* LUA_NUMBER */

#define issep(c) ((c) == '_' || (c) == '\'')

static void read_numeral (LexState *ls, SemInfo *seminfo) {
  int ch, usedsep, lastch, overflow, rc;
  usedsep = lastch = -1;
  overflow = 0;
  lua_assert(isdigit(ls->current));
  do {  /* 2.10.0 improved separator recognition */
    ch = ls->current;
    if (!issep(ch)) {  /* ignore single quotes or underscores in number; added 0.7.1, 2.5.1 (suggested by Slobodan) */
      save(ls, ch);
    } else {
      if (usedsep == -1) usedsep = ch;  /* save separator */
      if (ch != usedsep) luaX_lexerror(ls, "different separator", TK_NUMBER);  /* other separator used */
      if (ch == lastch) luaX_lexerror(ls, "duplicate separator", TK_NUMBER);  /* subsequent separator found */
    }
    next(ls);
    /* save_and_next(ls); */
    lastch = ch;
    ch = ls->current;
  } while (isdigit(ch) || ch == '.' || issep(ch));
  if (issep(lastch))  /* 2.10.0 */
    luaX_lexerror(ls, "trailing separator", TK_NUMBER);
  if (check_next(ls, "Ee")) /* `E'? */
    check_next(ls, "+-");  /* optional exponent sign */
  usedsep = lastch = -1;
  /* process postfix of scientific notation or hexadecimal float */
  while (1) {  /* 2.10.0, improved separator recognition */
    ch = ls->current;
    if (issep(ch)) {
      if (usedsep == -1) usedsep = ch;  /* save separator */
      if (ch != usedsep) luaX_lexerror(ls, "different separator", TK_NUMBER);  /* other separator used */
      if (ch == lastch) luaX_lexerror(ls, "duplicate separator", TK_NUMBER);  /* subsequent separator found */
    } else if (check_next(ls, "Pp")) {  /* for hexadecimal floats, 2.27.10 */
      check_next(ls, "+-");  /* optional exponent sign */
      if (isalnum(ls->current))
        save(ls, ls->current);
      else
        break;
    } else if (isalnum(ch))
      save(ls, ch);
    else if (ch == '.')
      save(ls, ch);
    else
      break;
    next(ls);
    lastch = ch;
  }
  if (issep(lastch))  /* 2.10.0 */
    luaX_lexerror(ls, "trailing separator", TK_NUMBER);
  save(ls, '\0');
  buffreplace(ls, '.', ls->decpoint);  /* follow locale for decimal point */
  rc = luaO_str2d(luaZ_buffer(ls->buff), &seminfo->r, &overflow);
  if (agn_getconstanttoobig(ls->L) && overflow) {  /* 2.39.4 */
    luaX_lexerror(ls, "constant too big", TK_NUMBER);
  }
  if (!rc)  /* format error? */
    trydecpoint(ls, seminfo, &overflow); /* try to update decimal point separator */
  if (agn_getconstanttoobig(ls->L) && overflow) {  /* 2.39.4 */
    luaX_lexerror(ls, "constant too big", TK_NUMBER);
  }
}


/* UTF-8 string input taken from Lua 5.4.8/Agena 6.1.0 */
static void lesccheck (LexState *ls, int c, const char *msg) {
  if (!c) {
    if (ls->current != EOZ)
      save_and_next(ls);  /* add current to buffer for error message */
    luaX_lexerror(ls, msg, TK_STRING);
  }
}

static int lgethexa (LexState *ls) {
  save_and_next(ls);
  lesccheck (ls, lisxdigit(ls->current), "hexadecimal digit expected");
  return luaO_hexavalue(ls->current);
}

static unsigned long readutf8esc (LexState *ls) {
  unsigned long r;
  int i = 4;  /* chars to be removed: '\', 'u', '{', and first digit */
  save_and_next(ls);  /* skip 'u' */
  lesccheck(ls, ls->current == '{', "missing '{'");
  r = lgethexa(ls);  /* must have at least one digit */
  while (cast_void(save_and_next(ls)), lisxdigit(ls->current)) {
    i++;
    lesccheck(ls, r <= (0x7FFFFFFFu >> 4), "UTF-8 value too large");
    r = (r << 4) + luaO_hexavalue(ls->current);
  }
  lesccheck(ls, ls->current == '}', "missing '}'");
  next(ls);  /* skip '}' */
  luaZ_buffremove(ls->buff, i);  /* remove saved chars from buffer */
  return r;
}

static void utf8esc (LexState *ls) {
  int i;
  char buff[UTF8BUFFSZ];
  for (i=0; i < UTF8BUFFSZ; i++) buff[i] = 0;  /* better sure than sorry */
  int n = luaO_utf8esc(buff, readutf8esc(ls));
  for (; n > 0; n--) {  /* add 'buff' to string */
    save(ls, buff[UTF8BUFFSZ - n]);
  }
}


static void escerror (LexState *ls, int *c, int n, const char *msg) {
  int i;
  luaZ_resetbuffer(ls->buff);  /* prepare error message */
  save(ls, '\\');
  for (i = 0; i < n && c[i] != EOZ; i++)
    save(ls, c[i]);
  luaX_lexerror(ls, msg, TK_STRING);
}

static int readhexaesc (LexState *ls) {
  int c[3], i;  /* keep input for error message */
  int r = 0;    /* result accumulator */
  c[0] = 'x';   /* for error message */
  for (i = 1; i < 3; i++) {  /* read two hexadecimal digits */
    c[i] = next(ls);
    if (!lisxdigit(c[i]))
      escerror(ls, c, i + 1, "hexadecimal digit expected");
    r = (r << 4) + luaO_hexavalue(c[i]);
  }
  return r;
}

static void read_long_string (LexState *ls, SemInfo *seminfo, int sep) {
  /* for comments and strings, optimised 1.6.7, improved 2.5.1 (suggested by Slobodan),
     improved 2.5.4 (suggested by a forum user) */
  int offset = 1;
  int escape = sep != '`';
  save_and_next(ls);  /* skip 2nd `[' */
  if (currIsNewline(ls))  /* string starts with a newline? */
    inclinenumber(ls);  /* skip it */
  for (;;) {
    switch (ls->current) {
      case EOZ:
        luaX_lexerror(ls, (seminfo) ? "unfinished long string" :
                                      "unfinished long comment", TK_EOS);
        break;  /* to avoid warnings */
      case '/': {
        next(ls);
        if (ls->current == '#') {  /* we have ending `/#' */
          if (seminfo) {  /* we are in a string, 2.21.3 fix to avoid segfaults */
            save(ls, '/');
            save_and_next(ls);  /* save `/#' into string */
            break;
          }
          if (comment_oldstyle > 0) comment_oldstyle--;
          if (sep != '#' && comment_oldstyle > 0) {
            luaX_lexerror(ls, "comment must end with " LUA_QL("*/"), 0);
          }
          save_and_next(ls);  /* skip `#' */
          goto endloop;
        } else
          save(ls, '/');
        break;
      }
      case '*': {  /* added 2.9.3 */
        next(ls);
        if (ls->current == '/') {  /* we have ending `***///' */
          if (seminfo) {  /* we are in a string, 2.21.3 fix to avoid segfaults */
            save(ls, '*');
            save_and_next(ls);  /* save `***///' into string */
            break;
          }
          if (comment_newstyle > 0) comment_newstyle--;
          if (sep != '/' && comment_newstyle > 0) {
            luaX_lexerror(ls, "comment must end with " LUA_QL("/#"), 0);
          }
          save_and_next(ls);  /* skip `/' */
          goto endloop;
        } else
          save(ls, '*');
        break;
      }
      case '\\': {
        int c;
        next(ls);  /* do not save the `\' */
        if (!escape) {  /* we will not escape since string started with a backquote; 2.39.0 */
          if (ls->current == 'q') {  /* the only exception is \q denoting a backquote */
            next(ls);
            save(ls, '`');
          } else
            save(ls, '\\');
          break;
        }
        switch (ls->current) {
          case 'a': c = '\a'; break;
          case 'b': c = '\b'; break;
          case 'e': c = '\e'; break;  /* the ESC character (ASCII 27), a POSIX extension but not Standard C (C90/C99). 5.6.1 */
          case 'f': c = '\f'; break;
          case 'n': c = '\n'; break;
          case 'r': c = '\r'; break;
          case 't': c = '\t'; break;
          case 'u': utf8esc(ls); offset = 0; goto no_save;  /* 6.1.0, set offset to 0 so that the first result character is not discarded */
          case 'v': c = '\v'; break;
          case 'x': c = readhexaesc(ls); goto read_save;  /* hex escapes, 2.22.0 */
          case '\n':  /* go through */
          case '\r': save(ls, '\n'); inclinenumber(ls); continue;
          case EOZ: continue;  /* will raise an error next loop */
          case 'z': {  /* 2.22.0, zap following span of spaces */
            next(ls);  /* skip the 'z' */
            while (lisspace(ls->current)) {
              if (currIsNewline(ls)) inclinenumber(ls);
              else next(ls);
            }
            goto no_save;
          }
          default: {
            if (!isdigit(ls->current)) {
              save_and_next(ls);  /* handles \\, \", \', and \? */
            } else {  /* \xxx */
              int i = 0;
              c = 0;
              do {
                c = 10*c + (ls->current - '0');
                next(ls);
              } while (++i < 3 && isdigit(ls->current));
              if (c > UCHAR_MAX)
                luaX_lexerror(ls, "escape sequence too large", TK_STRING);
              save(ls, c);
            }
            continue;
          }
        }
        read_save:
          save(ls, c);  /* save 'c' */
          next(ls);  /* read next character */
          continue;
        no_save:
          break;  /* 2.22.0 */
      }  /* end of case '\\' */
      case '\n':
      case '\r': {
        save(ls, '\n');
        inclinenumber(ls);
        if (!seminfo) luaZ_resetbuffer(ls->buff);  /* avoid wasting space */
        break;
      }
      case '"': case '\'': case '`': {  /* changed 2.10.0 */
        if (seminfo) {  /* are we processing a string or a comment ? */
          if (ls->current != sep) {  /* 2.6.0, suggested by a forum user to avoid using backslashes within strings */
            save(ls, ls->current);   /* we are not yet at the end of the string */
            next(ls);
            break;
          }
          save_and_next(ls);
          sep = 0;
          goto endloop;
        }  /* else go through */
      }
      default: {
        if (seminfo) save_and_next(ls);
        else next(ls);
      }
    }
  } endloop:
  if (seminfo) {
    seminfo->ts = luaX_newstring(ls, luaZ_buffer(ls->buff) + (offset + sep),  /* Valgrind, Agena 1.6.6 */
                                     luaZ_bufflen(ls->buff) - 2*(offset + sep) - (offset == 0));
  }
}


static int llex (LexState *ls, SemInfo *seminfo) {
  luaZ_resetbuffer(ls->buff);
  for (;;) {
    switch (ls->current) {
      case '\n':
      case '\r': {
        inclinenumber(ls);
        continue;
      }
      case '#': {   /* changed */
        next(ls);
        if (ls->current == '/') {  /* long comment, changed, Valgrind 1.6.6 */
          luaZ_resetbuffer(ls->buff);  /* `skip_sep' may dirty the buffer */
          read_long_string(ls, NULL, '#');  /* long comment */
          luaZ_resetbuffer(ls->buff);
          if (comment_oldstyle < 255) comment_oldstyle++;
          else luaX_lexerror(ls, "too many nested comments", 0);
          continue;
        }
        /* else short comment */
        while (!currIsNewline(ls) && ls->current != EOZ)
          next(ls);
        continue;
      }
      case '=': {
        next(ls);
        if (ls->current == '=') { next(ls); return TK_EEQ; }
        return TK_EQ;
      }
      case ':': {  /* changed */
        next(ls);
        if (ls->current == '=') { next(ls); return TK_ASSIGN; }
        if (ls->current == ':') { next(ls); return TK_DCOLON; }
        if (ls->current == '>') { next(ls); return TK_GT2; }
        return ':';
      }
      case '*': {  /* changed */
        next(ls);
        if (ls->current == '*') { next(ls); return TK_IPOW; }
        if (ls->current == '%') { next(ls); return TK_PERCENT; }
        if (ls->current == ':') {  /* 2.14.1 */
          next(ls);
          if (ls->current == '=') { next(ls); return TK_COMPMUL; }
        }
        return '*';
      }
      case '-': {  /* changed */
        next(ls);
        if (ls->current == '%') { next(ls); return TK_PERCENTSUB; }
        if (ls->current == '>') { next(ls); return TK_ARROW; }
        if (ls->current == '-') { next(ls);
          if (ls->current == '-') { next(ls); return TK_MEPS; }
          return TK_MM;
        }
        if (ls->current == ':') {  /* 2.14.1 */
          next(ls);
          if (ls->current == '=') { next(ls); return TK_COMPSUB; }
          else return TK_NOTOFTYPE;  /* 6.0.0 */
        } else if (ls->current == '?') {  /* 6.0.0 */
          next(ls);
          return TK_MINUSQMARK;
        }
        return '-';
      }
      case '%': {  /* 2.10.0 */
        next(ls);
        if (ls->current == '%') { next(ls); return TK_PERCENTCHANGE; }
        if (ls->current == ':') {  /* 2.14.1 */
          next(ls);
          if (ls->current == '=') { next(ls); return TK_COMPMOD; }
        }
        return '%';
      }
      case '<': {
        next(ls);
        /* changed Oct. 12, 2006 and June 17, 2007 */
        if (ls->current == '=') { next(ls); return TK_LE; }
        if (ls->current == '>') { next(ls); return TK_NEQ; }  /* changed 0.4.0 */
        if (ls->current == ':') { next(ls); return TK_LT2; }  /* short-cut function, new 6.0.0 */
        if (ls->current == '<') {
          next(ls);
          if (ls->current == '<') {  /* 2.3.0 RC 4, bitwise shift to the left */
            next(ls);
            if (ls->current == ':') {  /* 6.0.0 */
              next(ls);
              if (ls->current == '=') { next(ls); return TK_COMPLBROT; }
            }
            return TK_LBROTATE;  /* 2.5.4/6.0.0 */
          } else if (ls->current == ':') {  /* 6.0.0 */
            next(ls);
            if (ls->current == '=') { next(ls); return TK_COMPBLEFT; }
            return '<';
          }
          return TK_BLEFT;
        }
        return TK_LT;
      }
      case '>': {
        next(ls);
        if (ls->current == '=') { next(ls); return TK_GE; }
        if (ls->current == '>') {
          next(ls);
          if (ls->current == '>') {  /* 2.3.0 RC 4, bitwise shift to the right */
            next(ls);
            if (ls->current == ':') {  /* 6.0.0 */
              next(ls);
              if (ls->current == '=') { next(ls); return TK_COMPRBROT; }
            }
            return TK_RBROTATE;  /* 2.5.4 */
          } else if (ls->current == ':') {  /* 6.0.0 */
            next(ls);
            if (ls->current == '=') { next(ls); return TK_COMPBRIGHT; }
            return '>';
          }
          return TK_BRIGHT;
        }
        return TK_GT;
      }
      case '\'': case '\"': {  /* changed 0.25.5, 2.10.0, 2.38.4 */
        luaZ_resetbuffer(ls->buff);
        read_long_string(ls, seminfo, ls->current);
        return TK_STRING;
      }
      case '`': {  /* changed 0.25.5, 2.10.0, 2.38.4 */
        luaZ_resetbuffer(ls->buff);
        read_long_string(ls, seminfo, ls->current);
        return TK_STRING;
      }
      case '&': {  /* changed 0.4.0 */
        next(ls);
        if (ls->current == '&') {
          next(ls);
          if (ls->current == ':') {
            next(ls);
            if (ls->current == '=') { next(ls); return TK_COMPBAND; }
            return '&';
          }
          return TK_BAND;
        }
        if (ls->current == '+')  { next(ls); return TK_I32ADD; }
        if (ls->current == '-')  { next(ls); return TK_I32SUB; }
        if (ls->current == '*')  { next(ls); return TK_I32MUL; }
        if (ls->current == '/')  { next(ls); return TK_I32DIV; }
        if (ls->current == '\\') { next(ls); return TK_I32INTDIV; }
        if (ls->current == ':') {
          next(ls);
          if (ls->current == '=') {
            next(ls);
            return TK_COMPCAT;  /* 2.14.11 */
          }
          return '&';  /* TK_CONCAT; */ /* changed 2.15.0, 6.0.0 */
        }
        return TK_CONCAT;
      }
      case '|': {  /* changed 0.4.0 */
        next(ls);
        if (ls->current == '|') {
          next(ls);
          if (ls->current == ':') {
            next(ls);
            if (ls->current == '=') { next(ls); return TK_COMPBOR; }
            return '|';
          }
          return TK_BOR;
        }
        if (ls->current == '-') { next(ls); return TK_ABSDIFF; }
        return '|';
      }
      case '^': {  /* changed 0.4.0 */
        next(ls);
        if (ls->current == '^') {
          next(ls);
          if (ls->current == ':') {
            next(ls);
            if (ls->current == '=') { next(ls); return TK_COMPBXOR; }
            return '^';
          }
          return TK_BXOR;
        }
        return '^';
      }
      case '/': {  /* changed 1.10.6 */
        next(ls);
        if (ls->current == '*') {  /* C-style long comment, added 2.9.3 */
          luaZ_resetbuffer(ls->buff);  /* `skip_sep' may dirty the buffer */
          read_long_string(ls, NULL, '/');  /* long comment */
          luaZ_resetbuffer(ls->buff);
          if (comment_newstyle < 255) comment_newstyle++;
          else luaX_lexerror(ls, "too many nested comments", 0);
          continue;
        }
        if (ls->current == '%') { next(ls); return TK_PERCENTRATIO; }
        if (ls->current == '/') { next(ls); return TK_DATA; }  /* 2.9.3 */
        if (ls->current == ':') {  /* 2.14.1 */
          next(ls);
          if (ls->current == '=') { next(ls); return TK_COMPDIV; }
        }
        return '/';
      }
      case '\\': {  /* added 2.9.3 */
        next(ls);
        if (ls->current == '\\') { next(ls); return TK_ATAD; }
        if (ls->current == ')') { next(ls); return TK_SEQDATAEND; }  /* 2.10.0 */
        if (ls->current == ':') {
          next(ls);
          if (ls->current == '=') { next(ls); return TK_COMPINTDIV; }  /* 2.15.0 */
        }
        return '\\';
      }
      case '(': {  /* added 2.10.0 */
        next(ls);
        if (ls->current == '/') { next(ls); return TK_SEQDATA; }
        else return '(';
      }
      case '+': {  /* changed 1.11.3 */
        next(ls);
        if (ls->current == '%') { next(ls); return TK_PERCENTADD; }
        if (ls->current == '+') {
          next(ls);
          if (ls->current == '+') { next(ls); return TK_PEPS; }
          return TK_PP;
        }
        if (ls->current == ':') {  /* 2.14.1 */
          next(ls);
          if (ls->current == '=') { next(ls); return TK_COMPADD; }
        }
        return '+';
      }
      case '~': {  /* changed 0.4.0 */
        next(ls);
        if (ls->current == '~') { next(ls); return TK_BNOT; }
        else if (ls->current == '=') { next(ls); return TK_AEQ; }
        else if (ls->current == '|') { next(ls); return TK_ACOMPARE; }
        else if (ls->current == '<') {
          next(ls);
          if (ls->current == '>') {
            next(ls);
            return TK_NAEQ;
          }
          return '~';
        }
        return TK_SEP;
      }
      case '@': {  /* changed 0.4.0 */
        next(ls);
        if (ls->current == '@') {
          next(ls);
          return TK_OOP;
        }
        if (ls->current == ':') {  /* 2.29.2 */
          next(ls);
          if (ls->current == '=') { next(ls); return TK_COMPMULOP; }
          return '@';
        }
        return TK_MAP;
      }
      case '$': {  /* changed 2.2.x, 2.23.0, 4.6.0 */
        next(ls);
        if (ls->current == '$') {
          next(ls);
          if (ls->current == '$') { next(ls); return TK_COUNT; }
          return TK_HAS;
        }
        return TK_SELECT;
      }
      case '!': {  /* added 3.10.5 */
        next(ls);
        if (ls->current == '!') { next(ls); return TK_CARTESIAN; }
        return TK_COMPLEX;
      }
      case '.': {
        save_and_next(ls);
        if (ls->current == '.') { next(ls); return TK_DD; }
        if (!isdigit(ls->current)) return '.';
        else {
          read_numeral(ls, seminfo);
          return TK_NUMBER;
        }
      }
      case EOZ: {
        return TK_EOS;
      }
      case '?': {  /* new 2.21.3, changed 6.0.0 */
        next(ls);
        return TK_QMARK;
      }
      default: {
        if (isspace(ls->current)) {
          lua_assert(!currIsNewline(ls));
          next(ls);
          continue;
        }
        else if (isdigit(ls->current)) {
          read_numeral(ls, seminfo);
          return TK_NUMBER;
        }
        else if ((tools_alphadia[ls->current + 1] & (__ALPHA | __DIACR)) || ls->current == '_' ) {  /* changed 0.4.1, 2.8.0, 2.8.3 patch */
          /* identifier or reserved word */
          TString *ts;
          do {
            save_and_next(ls);
          } while ((tools_alphadia[ls->current + 1] &
                   (__ALPHA | __DIACR)) || isdigit (ls->current) || ls->current == '_' || ls->current == '\'');  /* changed 0.4.1, 2.8.0, 2.10.5 */
          ts = luaX_newstring(ls, luaZ_buffer(ls->buff),
                                  luaZ_bufflen(ls->buff));
          if (ts->tsv.reserved > 0)  /* reserved word? */
            return ts->tsv.reserved - 1 + FIRST_RESERVED;
          else {
            seminfo->ts = ts;
            return TK_NAME;
          }
        }
        else {
          int c = ls->current;
          next(ls);
          return c;  /* single-char tokens (+ - / ...) */
        }
      }
    }
  }
}


int luaX_next (LexState *ls) {
  ls->lastline = ls->linenumber;
  if (ls->lookahead.token != TK_EOS) {  /* is there a look-ahead token? */
    ls->t = ls->lookahead;  /* use this one */
    ls->lookahead.token = TK_EOS;  /* and discharge it */
  }
  else {
    ls->t.token = llex(ls, &ls->t.seminfo);  /* read next token */
  }
  return ls->t.token;  /* 6.1.0 */
}


int luaX_lookahead (LexState *ls) {
  lua_assert(ls->lookahead.token == TK_EOS);
  ls->lookahead.token = llex(ls, &ls->lookahead.seminfo);
  return ls->lookahead.token;  /* 2.20.2 */
}

