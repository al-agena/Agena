/* inifile library
   taken from: https://gitlab.com/iniparser/iniparser

   For creator and licence, see iniparse.h.

   @file  dictionary.c
   @author  N. Devillard
   @brief   Implements a dictionary for string variables.

   This module implements a simple dictionary object, i.e. a list
   of string/string associations. This object is useful to store e.g.
   information retrieved from a configuration file (ini files).
*/

/*---------------------------------------------------------------------------
                Includes
 ---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <inttypes.h>

#define iniparse_c
#define LUA_LIB

#include "agena.h"
#include "agnxlib.h"
#include "agnhlps.h"
#include "agenalib.h"
#include "lobject.h"   /* for luaO_str2d */

#include "iniparse.h"


/*---------------------------------------------------------------------------
              Private functions
 ---------------------------------------------------------------------------*/

/**
  @brief  Duplicate a string
  @param  s String to duplicate
  @return   Pointer to a newly allocated string, to be freed with free()

  This is a replacement for strdup(). This implementation is provided
  for systems that do not have it.
*/
static char *xstrdup (const char *s) {
  char *t;
  size_t len;
  if (!s) return NULL;
  len = tools_strlen(s) + 1;
  t = (char *)malloc(len*sizeof(char));  /* 4.6.7 change */
  if (t) tools_memcpy(t, s, len);
  return t;
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Double the size of the dictionary
  @param  d Dictionary to grow
  @return   This function returns non-zero in case of failure
*/
static int dictionary_grow (dictionary *d) {
  char **new_val, **new_key;
  unsigned *new_hash;
  size_t newsize = agn_newsize(NULL, d->size);  /* 4.6.8 */
  new_val  = (char **)calloc(newsize, sizeof *d->val);
  new_key  = (char **)calloc(newsize, sizeof *d->key);
  new_hash = (unsigned int *)calloc(newsize, sizeof *d->hash);
  if (!new_val || !new_key || !new_hash) {
    /* An allocation failed, leave the dictionary unchanged */
    if (new_val)  { xfree(new_val); }
    if (new_key)  { xfree(new_key); }
    if (new_hash) { xfree(new_hash); }
    return -1;
  }
  /* Initialize the newly allocated space */
  tools_memcpy(new_val, d->val, d->size * sizeof(char *));
  tools_memcpy(new_key, d->key, d->size * sizeof(char *));
  tools_memcpy(new_hash, d->hash, d->size * sizeof(unsigned int));
  /* Delete previous data */
  xfree(d->val);
  xfree(d->key);
  xfree(d->hash);
  /* Actually update the dictionary */
  d->size = newsize;
  d->val = new_val;
  d->key = new_key;
  d->hash = new_hash;
  return 0;
}

/*---------------------------------------------------------------------------
              Function codes
 ---------------------------------------------------------------------------*/

/**
  @brief  Compute the hash key for a string.
  @param  key   Character string to use for key.
  @return   1 unsigned int on at least 32 bits.

  This hash function has been taken from an Article in Dr Dobbs Journal.
  This is normally a collision-free function, distributing keys evenly.
  The key is stored anyway in the struct so that collision can be avoided
  by comparing the key itself in last resort.
*/
unsigned int dictionary_hash (const char *key) {
  size_t len;
  unsigned hash;
  size_t i;
  if (!key) return 0;
  len = tools_strlen(key);
  for (hash=0, i=0; i < len; i++) {
    hash += (unsigned int)key[i];
    hash += (hash << 10);
    hash ^= (hash >> 6);
  }
  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);
  return hash;
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Create a new dictionary object.
  @param  size  Optional initial size of the dictionary.
  @return   1 newly allocated dictionary object.

  This function allocates a new dictionary object of given size and returns
  it. If you do not know in advance (roughly) the number of entries in the
  dictionary, give size=0.
*/
dictionary *dictionary_new (size_t size) {
  size_t i;
  dictionary *d;
  /* If no size was specified, allocate space for DICTMINSZ */
  if (size < DICTMINSZ) size = DICTMINSZ;
  d = (dictionary *)calloc(1, sizeof *d);
  if (d) {
    d->size = size;
    d->bucketslots = BUCKETSLOTS;
    d->val  = (char **)calloc(size, sizeof *d->val);
    d->key  = (char **)calloc(size, sizeof *d->key);
    d->hash = (unsigned int *)calloc(size, sizeof *d->hash);
    d->nextfree = (unsigned int *)calloc(BUCKETSLOTS, sizeof *d->nextfree);
    if (!d->size || !d->val || !d->hash || !d->nextfree) {  /* 4.6.9 fix */
      /* free((void *)d->size); */  /* you can't free an atomic value */
      free((void *)d->val);
      free((void *)d->hash);
      free((void *)d->nextfree);
      xfree(d);
    }
    for (i=0; i < BUCKETSLOTS; i++) d->nextfree[i] = i;  /* next free position to occupy */
  }
  return d;
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Delete a dictionary object
  @param  d   dictionary object to deallocate.
  @return   void

  If xfreeme is 1, deallocate a dictionary object and all memory associated to it.
  This is for non-userdata use.

  If xfreeme is 0, free all memory associated to it, but not the dictiionary itself, as
  this breaks the garbage collector.
*/
void dictionary_del (dictionary *d, int xfreeme) {
  size_t i;
  if (d == NULL) return;
  for (i=0; i < d->size; i++) {
    if (d->key[i] != NULL) free(d->key[i]);
    if (d->val[i] != NULL) free(d->val[i]);
  }
  free(d->val);
  free(d->key);
  free(d->hash);
  free(d->nextfree);  /* 4.6.8 */
  if (xfreeme) { xfree(d); }
  return;
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Get a value from a dictionary.
  @param  d     dictionary object to search.
  @param  key   Key to look for in the dictionary.
  @param  def   Default value to return if key not found.
  @return   1 pointer to internally allocated character string.
  @return rc:   -1 if key could not be found, 0 if key actually denotes a section name,
                +1 if key is a key name.

  This function locates a key in a dictionary and returns a pointer to its
  value, or the passed 'def' pointer if no such key can be found in
  dictionary. The returned character pointer points to data internal to the
  dictionary object, you should not try to free it or modify it.
*/
const char *dictionary_get (dictionary *d, const char *key, const char *def, int *rc) {
  unsigned int hash, offset, top;
  size_t i;
  *rc = -1;
  if (d == NULL || key == NULL) return def;
  /* Compute hash for this key and slot offset in bucket, changed for Agena 4.6.7 */
  hash = dictionary_hash(key);
  offset = hash & (d->bucketslots - 1);
  top = fMin(d->size, d->nextfree[offset]);  /* 4.6.8 tweak */
  for (i=offset; i < top; i += d->bucketslots) {
    /* Compare hash and compare string, to avoid hash collisions, 4.6.7 change */
    if (d->key[i] && hash == d->hash[i] && tools_streq(key, d->key[i])) {
      *rc = d->val[i] ? 1 : 0;
      return d->val[i];
    }
  }
  return def;
}


/*-------------------------------------------------------------------------*/
/**
  @brief  Set a value in a dictionary.
  @param  d     dictionary object to modify.
  @param  key   Key to modify or add.
  @param  val   Value to add.
  @return   int   0 if Ok, anything else otherwise

  If the given key is found in the dictionary, the associated value is
  replaced by the provided one. If the key cannot be found in the
  dictionary, it is added to it.

  It is Ok to provide a NULL value for val, but NULL values for the dictionary
  or the key are considered as errors: the function will return immediately
  in such a case.

  Notice that if you dictionary_set a variable to NULL, a call to
  dictionary_get will return a NULL value: the variable will be found, and
  its value (NULL) is returned. In other words, setting the variable
  content to NULL is equivalent to deleting the variable from the
  dictionary. It is not possible (in this implementation) to have a key in
  the dictionary without value.

  This function returns non-zero in case of failure.
*/
int dictionary_set (dictionary *d, const char *key, const char *val) {
  unsigned int hash, offset, i, top;
  if (d == NULL || key == NULL) return -1;
  /* Compute hash for this key and slot number offset in bucket */
  hash = dictionary_hash(key);
  offset = hash & (d->bucketslots - 1);
  top = fMin(d->size, d->nextfree[offset]);
  /* Find if value is already in dictionary */
  if (d->n > 0) {
    for (i=offset; i < top; i += d->bucketslots) {
      if (d->key[i] && hash == d->hash[i] && tools_streq(key, d->key[i])) { /* Same hash value and same key */
        /* Found a value: modify and return */
        if (d->val[i] != NULL) xfree(d->val[i]);
        d->val[i] = (val ? xstrdup(val) : NULL);
        /* Value has been modified: return */
        return 0;
      }
    }
  }
  /* Add a new value in the first empty slot. Internal memory management has been completely changed from here for
     the Agena edition. 4.6.7 */
  i = d->nextfree[offset];
  if (i + d->bucketslots >= d->size) {  /* See if dictionary needs to grow */
    /* maximum size is near: reallocate dictionary */
    if (dictionary_grow(d) != 0) return -1;
  }
  while (d->key[i]) i += d->bucketslots;  /* check for assigned slot: better sure than sorry */
  /* Copy key */
  d->key[i]  = xstrdup(key);
  d->val[i]  = (val ? xstrdup(val) : NULL);
  d->hash[i] = hash;
  d->n++;
  d->nextfree[offset] = i + d->bucketslots;  /* 4.6.8 fix */
  return 0;
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Delete a key in a dictionary
  @param  d     dictionary object to modify.
  @param  key   Key to remove.
  @return int (formerly void)

  This function deletes a key in a dictionary. Nothing is done if the
  key cannot be found.

  Added int return values for Agena binding. 4.6.7
*/
int dictionary_unset (dictionary *d, const char *key) {
  unsigned int hash, offset, i, top;
  if (key == NULL || d == NULL) return -1;
  hash = dictionary_hash(key);
  offset = hash & (d->bucketslots - 1);
  top = fMin(d->size, d->nextfree[offset]);
  for (i=offset; i < top; i += d->bucketslots) {  /* 4.6.7 change */
    /* Compare hash and compare string, to avoid hash collisions */
    if (d->key[i] && hash == d->hash[i] && tools_streq(key, d->key[i])) break;  /* Found key */
  }
  if (i >= top) return 1;  /* Key not found */
  xfree(d->key[i]);
  if (d->val[i] != NULL) {
    xfree(d->val[i]);
  }
  d->hash[i] = 0;
  d->n--;
  return 0;
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Dump a dictionary to an opened file pointer.
  @param  d   Dictionary to dump
  @param  f   Opened file pointer.
  @return   void

  Dumps a dictionary onto an opened file pointer. Key pairs are printed out
  as @c [Key]=[Value], one per line. It is Ok to provide stdout or stderr as
  output file pointers.
*/
void dictionary_dump (dictionary *d, FILE *out) {
  size_t i;
  if (d == NULL || out == NULL) return;
  if (d->n < 1) {
    fprintf(out, "empty dictionary\n");
    return;
  }
  for (i=0; i < d->size; i++) {
    if (d->key[i]) {
      fprintf(out, "%20s\t[%s]\n", d->key[i], d->val[i] ? d->val[i] : "UNDEF");
    }
  }
  return;
}

/* ######################################################################## */

/*-------------------------------------------------------------------------*/
/**
   @file  iniparser.c
   @author  N. Devillard
   @brief   Parser for ini files.
*/

/*---------------------------------------------------------------------------
            Private to this module
 ---------------------------------------------------------------------------*/
/**
 * This enum stores the status for each parsed line (internal use only).
 */
typedef enum _line_status_ {
  LINE_UNPROCESSED,
  LINE_ERROR,
  LINE_EMPTY,
  LINE_COMMENT,
  LINE_SECTION,
  LINE_VALUE
} line_status;

/*-------------------------------------------------------------------------*/
/**
  @brief  Convert a string to lowercase.
  @param  in   String to convert.
  @param  out Output buffer.
  @param  len Size of the out buffer.
  @return   ptr to the out buffer or NULL if an error occurred.

  This function convert a string into lowercase.
  At most len - 1 elements of the input string will be converted.
*/
static const char *strlwc (const char *in, char *out, unsigned len) {
  unsigned i;
  if (in == NULL || out == NULL || len == 0) return NULL;
  i=0;
  while (in[i] != '\0' && i < len - 1) {
    out[i] = (char)tolower((int)in[i]);
    i++;
  }
  out[i] = '\0';
  return out;
}

/**
  @brief  Remove blanks at the beginning and the end of a string.
  @param  str  String to parse and alter.
  @return   unsigned New size of the string.
*/
static unsigned strstrip (char *s) {
  char *last = NULL;
  char *dest = s;
  if (s == NULL) return 0;
  last = s + tools_strlen(s);
  while (isspace((int)*s) && *s) s++;
  while (last > s) {
    if (!isspace((int)*(last-1)))
      break;
    last --;
  }
  *last = (char)0;
  memmove(dest, s, last - s + 1);
  return last - s;
}

/**
  @brief  Default error callback for iniparser: wraps `fprintf(stderr, ...)`.
*/
static int default_error_callback (const char *format, ...) {
  int ret;
  va_list argptr;
  va_start(argptr, format);
  ret = vfprintf(stderr, format, argptr);
  va_end(argptr);
  return ret;
}

static int (*iniparser_error_callback)(const char *, ...) = default_error_callback;

/**
  @brief  Configure a function to receive the error messages.
  @param  errback  Function to call.

  By default, the error will be printed on stderr. If a null pointer is passed
  as errback the error callback will be switched back to default.
*/
void iniparser_set_error_callback (int (*errback)(const char *, ...)) {
  if (errback) {
    iniparser_error_callback = errback;
  } else {
    iniparser_error_callback = default_error_callback;
  }
}

/**
  @brief  Get number of sections in a dictionary
  @param  d   Dictionary to examine
  @return   int Number of sections found in dictionary

  This function returns the number of sections found in a dictionary.
  The test to recognize sections is done on the string stored in the
  dictionary: a section name is given as "section" whereas a key is
  stored as "section:key", thus the test looks for entries that do not
  contain a colon.

  This clearly fails in the case a section name contains a colon, but
  this should simply be avoided.

  This function returns -1 in case of error.
*/
int iniparser_getnsec (dictionary *d) {
  size_t i;
  int nsec;
  if (d == NULL) return -1;
  nsec = 0;
  for (i=0; i < d->size; i++) {
    if (d->key[i] && strchr(d->key[i], ':') == NULL) nsec++;  /* 4.6.9 tweak */
  }
  return nsec;
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Get name for section n in a dictionary.
  @param  d   Dictionary to examine
  @param  n   Section number (from 0 to nsec-1).
  @return   Pointer to char string

  This function locates the n-th section in a dictionary and returns
  its name as a pointer to a string statically allocated inside the
  dictionary. Do not free or modify the returned string!

  This function returns NULL in case of error.
*/
const char *iniparser_getsecname (dictionary *d, int n) {
  size_t i;
  int foundsec;
  if (d == NULL || n < 0) return NULL;
  foundsec = 0;
  for (i=0; i < d->size; i++) {
    if (d->key[i] && strchr(d->key[i], ':') == NULL) {  /* 4.6.9 change */
      foundsec++;
      if (foundsec > n) break;
    }
  }
  return (foundsec <= n) ? NULL : d->key[i];
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Dump a dictionary to an opened file pointer.
  @param  d   Dictionary to dump.
  @param  f   Opened file pointer to dump to.
  @return   void

  This function prints out the contents of a dictionary, one element by
  line, onto the provided file pointer. It is OK to specify @c stderr
  or @c stdout as output files. This function is meant for debugging
  purposes mostly.
*/
void iniparser_dump (dictionary *d, FILE *f) {
  size_t i;
  if (d == NULL || f == NULL) return;
  for (i=0; i < d->size; i++) {
    if (d->key[i] == NULL) continue;
    if (d->val[i] != NULL) {
      fprintf(f, "[%s]=[%s]\n", d->key[i], d->val[i]);
    } else {
      fprintf(f, "[%s]=UNDEF\n", d->key[i]);
    }
  }
  return;
}

static void escape_value (char *escaped, char *value) {
  char c;
  int v = 0;
  int e = 0;
  if (!escaped || !value) return;
  while ((c = value[v]) != '\0') {
    if (c == '\\' || c == '"') {
      escaped[e] = '\\';
      e++;
    }
    escaped[e] = c;
    v++;
    e++;
  }
  escaped[e] = '\0';
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Save a dictionary to a loadable ini file
  @param  d   Dictionary to dump
  @param  f   Opened file pointer to dump to
  @return   void

  This function dumps a given dictionary into a loadable ini file.
  It is Ok to specify @c stderr or @c stdout as output files.
*/
void iniparser_dump_ini (dictionary *d, FILE *f) {
  size_t i, nsec;
  const char *secname;
  char escaped[ASCIILINESZ + 1] = "";
  if (d == NULL || f == NULL) return;
  nsec = iniparser_getnsec(d);
  if (nsec < 1) {
    /* No section in file: dump all keys as they are */
    for (i=0; i < d->size; i++) {
      if (d->key[i] == NULL) continue;
      escape_value(escaped, d->val[i]);
      fprintf(f, "%s = \"%s\"\n", d->key[i], escaped);
    }
    return;
  }
  for (i=0; i < nsec; i++) {
    secname = iniparser_getsecname(d, i);
    iniparser_dumpsection_ini(d, secname, f);
  }
  fprintf(f, "\n");
  return;
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Save a dictionary section to a loadable ini file
  @param  d   Dictionary to dump
  @param  s   Section name of dictionary to dump
  @param  f   Opened file pointer to dump to
  @return   void

  This function dumps a given section of a given dictionary into a loadable ini
  file.  It is Ok to specify @c stderr or @c stdout as output files.
*/
void iniparser_dumpsection_ini (dictionary *d, const char *s, FILE *f) {
  size_t  j;
  char  keym[ASCIILINESZ + 1];
  int   seclen;
  char escaped[ASCIILINESZ + 1] = "";
  if (d == NULL || f == NULL) return;
  if (!iniparser_find_entry(d, s)) return;
  seclen  = (int)tools_strlen(s);
  fprintf(f, "\n[%s]\n", s);
  sprintf(keym, "%s:", s);
  for (j=0; j < d->size; j++) {
    if (d->key[j] && !strncmp(d->key[j], keym, seclen + 1)) {  /* 4.6.9 tweak */
      escape_value(escaped, d->val[j]);
      fprintf(f, "%-30s = \"%s\"\n", d->key[j]+seclen+1, escaped);
    }
  }
  fprintf(f, "\n");
  return;
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Get the number of keys in a section of a dictionary.
  @param  d   Dictionary to examine
  @param  s   Section name of dictionary to examine
  @return   Number of keys in section
*/
int iniparser_getsecnkeys (dictionary *d, const char *s) {
  int seclen, nkeys;
  char keym[ASCIILINESZ + 1];
  size_t j;
  nkeys = 0;
  if (d == NULL) return nkeys;
  if (!iniparser_find_entry(d, s)) return nkeys;
  seclen = (int)tools_strlen(s);
  strlwc(s, keym, sizeof(keym));
  keym[seclen] = ':';
  for (j=0; j < d->size; j++) {
    if (d->key[j] && !strncmp(d->key[j], keym, seclen + 1)) nkeys++;  /* 4.6.9 change */
  }
  return nkeys;
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Get the number of keys in a section of a dictionary.
  @param  d  Dictionary to examine
  @param  s  Section name of dictionary to examine
  @param  keys Already allocated array to store the keys in
  @return   The pointer passed as `keys` argument or NULL in case of error

  This function queries a dictionary and finds all keys in a given section.
  The keys argument should be an array of pointers which size has been
  determined by calling `iniparser_getsecnkeys` function prior to this one.

  Each pointer in the returned char pointer-to-pointer is pointing to
  a string allocated in the dictionary; do not free or modify them.
*/
const char **iniparser_getseckeys (dictionary *d, const char *s, const char **keys) {
  size_t i, j, seclen;
  char keym[ASCIILINESZ + 1];
  if (d == NULL || keys == NULL) return NULL;
  if (!iniparser_find_entry(d, s)) return NULL;
  seclen = tools_strlen(s);
  strlwc(s, keym, sizeof(keym));
  keym[seclen] = ':';
  i = 0;
  for (j=0; j < d->size; j++) {
    if (d->key[j] == NULL) continue;
    if (!strncmp(d->key[j], keym, seclen + 1)) {
      keys[i] = d->key[j];
      i++;
    }
  }
  return keys;
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Get the string associated to a key
  @param  d     Dictionary to search
  @param  key   Key string to look for
  @param  def   Default value to return if key not found.
  @return   pointer to statically allocated character string

  This function queries a dictionary for a key. A key as read from an
  ini file is given as "section:key". If the key cannot be found,
  the pointer passed as 'def' is returned.
  The returned char pointer is pointing to a string allocated in
  the dictionary, do not free or modify it.
*/
const char *iniparser_getstring (dictionary *d, const char *key, const char *def) {
  int rc;
  const char *lc_key, *sval;
  char tmp_str[ASCIILINESZ + 1];
  if (d == NULL || key == NULL) return def;
  lc_key = strlwc(key, tmp_str, sizeof(tmp_str));
  sval = dictionary_get(d, lc_key, def, &rc);
  return sval;
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Get the string associated to a key, convert to an long int
  @param  d Dictionary to search
  @param  key Key string to look for
  @param  notfound Value to return in case of error
  @return   long integer

  This function queries a dictionary for a key. A key as read from an
  ini file is given as "section:key". If the key cannot be found,
  the notfound value is returned.

  Supported values for integers include the usual C notation
  so decimal, octal (starting with 0) and hexadecimal (starting with 0x)
  are supported. Examples:

  "42"    ->  42
  "042"   ->  34 (octal -> decimal)
  "0x42"  ->  66 (hexa  -> decimal)

  Warning: the conversion may overflow in various ways. Conversion is
  totally outsourced to strtol(), see the associated man page for overflow
  handling.

  Credits: Thanks to A. Becker for suggesting strtol()
*/
long int iniparser_getlongint (dictionary *d, const char *key, long int notfound) {
  const char *str;
  str = iniparser_getstring(d, key, INI_INVALID_KEY);
  if (str == NULL || str == INI_INVALID_KEY) return notfound;
  return strtol(str, NULL, 0);
}

int64_t iniparser_getint64(dictionary *d, const char *key, int64_t notfound) {
  const char *str;
  str = iniparser_getstring(d, key, INI_INVALID_KEY);
  if (str == NULL || str == INI_INVALID_KEY) return notfound;
  return strtoimax(str, NULL, 0);
}

uint64_t iniparser_getuint64(dictionary *d, const char *key, uint64_t notfound) {
  const char *str;
  str = iniparser_getstring(d, key, INI_INVALID_KEY);
  if (str == NULL || str == INI_INVALID_KEY) return notfound;
  return strtoumax(str, NULL, 0);
}


/*-------------------------------------------------------------------------*/
/**
  @brief  Get the string associated to a key, convert to an int
  @param  d Dictionary to search
  @param  key Key string to look for
  @param  notfound Value to return in case of error
  @return   integer

  This function queries a dictionary for a key. A key as read from an
  ini file is given as "section:key". If the key cannot be found,
  the notfound value is returned.

  Supported values for integers include the usual C notation
  so decimal, octal (starting with 0) and hexadecimal (starting with 0x)
  are supported. Examples:

  "42"    ->  42
  "042"   ->  34 (octal -> decimal)
  "0x42"  ->  66 (hexa  -> decimal)

  Warning: the conversion may overflow in various ways. Conversion is
  totally outsourced to strtol(), see the associated man page for overflow
  handling.

  Credits: Thanks to A. Becker for suggesting strtol()
*/
int iniparser_getint (dictionary *d, const char *key, int notfound) {
  return (int)iniparser_getlongint(d, key, notfound);
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Get the string associated to a key, convert to a double
  @param  d Dictionary to search
  @param  key Key string to look for
  @param  notfound Value to return in case of error
  @return   double

  This function queries a dictionary for a key. A key as read from an
  ini file is given as "section:key". If the key cannot be found,
  the notfound value is returned.
*/
double iniparser_getdouble (dictionary *d, const char *key, double notfound) {
  const char *str;
  str = iniparser_getstring(d, key, INI_INVALID_KEY);
  if (str == NULL || str == INI_INVALID_KEY) return notfound;
  return atof(str);
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Get the string associated to a key, convert to a boolean
  @param  d Dictionary to search
  @param  key Key string to look for
  @param  notfound Value to return in case of error
  @return   integer

  This function queries a dictionary for a key. A key as read from an
  ini file is given as "section:key". If the key cannot be found,
  the notfound value is returned.

  A true boolean is found if one of the following is matched:

  - A string starting with 'y'
  - A string starting with 'Y'
  - A string starting with 't'
  - A string starting with 'T'
  - A string starting with '1'

  A false boolean is found if one of the following is matched:

  - A string starting with 'n'
  - A string starting with 'N'
  - A string starting with 'f'
  - A string starting with 'F'
  - A string starting with '0'

  The notfound value returned if no boolean is identified, does not
  necessarily have to be 0 or 1.
*/
int iniparser_getboolean (dictionary *d, const char *key, int notfound) {
  int ret;
  const char *c;
  c = iniparser_getstring(d, key, INI_INVALID_KEY);
  if (c == NULL || c == INI_INVALID_KEY) return notfound;
  if (c[0] == 'y' || c[0] == 'Y' || c[0] == '1' || c[0] == 't' || c[0] == 'T') {
    ret = 1;
  } else if (c[0] == 'n' || c[0] == 'N' || c[0] == '0' || c[0] == 'f' || c[0] == 'F') {
    ret = 0;
  } else {
    ret = notfound;
  }
  return ret;
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Finds out if a given entry exists in a dictionary
  @param  ini   Dictionary to search
  @param  entry   Name of the entry to look for
  @return   integer 1 if entry exists, 0 otherwise

  Finds out if a given entry exists in the dictionary. Since sections
  are stored as keys with NULL associated values, this is the only way
  of querying for the presence of sections in a dictionary.
*/
int iniparser_find_entry (dictionary *ini, const char *entry) {
  int found = 0;
  if (iniparser_getstring(ini, entry, INI_INVALID_KEY) != INI_INVALID_KEY) found = 1;
  return found;
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Set an entry in a dictionary.
  @param  ini   Dictionary to modify.
  @param  entry   Entry to modify (entry name)
  @param  val   New value to associate to the entry.
  @return   int 0 if Ok, -1 otherwise.

  If the given entry can be found in the dictionary, it is modified to
  contain the provided value. If it cannot be found, the entry is created.
  It is Ok to set val to NULL.
*/
int iniparser_set (dictionary *ini, const char *entry, const char *val) {
  char tmp_str[ASCIILINESZ + 1];
  return dictionary_set(ini, strlwc(entry, tmp_str, sizeof(tmp_str)), val);
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Delete an entry in a dictionary
  @param  ini   Dictionary to modify
  @param  entry   Entry to delete (entry name)
  @return   void

  If the given entry can be found, it is deleted from the dictionary.
*/
void iniparser_unset (dictionary *ini, const char *entry) {
  char tmp_str[ASCIILINESZ+1];
  dictionary_unset(ini, strlwc(entry, tmp_str, sizeof(tmp_str)));
}

static void parse_quoted_value (char *value, char quote) {
  char c, *quoted;
  int q = 0, v = 0;
  int esc = 0;
  if (!value) return;
  quoted = xstrdup(value);
  if (!quoted) {
    iniparser_error_callback("iniparser: memory allocation failure\n");
    goto end_of_value;
  }
  while ((c = quoted[q]) != '\0') {
    if (!esc) {
      if (c == '\\') {
        esc = 1;
        q++;
        continue;
      }
      if (c == quote) goto end_of_value;
    }
    esc = 0;
    value[v] = c;
    v++;
    q++;
  }
end_of_value:
  value[v] = '\0';
  xfree(quoted);
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Load a single line from an INI file
  @param  input_line  Input line, may be concatenated multi-line input
  @param  section   Output space to store section
  @param  key     Output space to store key
  @param  value     Output space to store value
  @return   line_status value
*/
static line_status iniparser_line (const char *input_line, char *section, char *key, char *value) {
  line_status sta;
  char *line = NULL;
  size_t len;
  int d_quote;
  line = xstrdup(input_line);
  len = strstrip(line);
  sta = LINE_UNPROCESSED;
  if (len < 1) {
    /* Empty line */
    sta = LINE_EMPTY;
  } else if (line[0] == '#' || line[0] == ';') {
    /* Comment line */
    sta = LINE_COMMENT;
  } else if (line[0] == '[' && line[len - 1] == ']') {
    /* Section name without opening square bracket */
    sscanf(line, "[%[^\n]", section);
    len = tools_strlen(section);
    /* Section name without closing square bracket */
    if (section[len - 1] == ']') section[len - 1] = '\0';
    strstrip(section);
    strlwc(section, section, len);
    sta = LINE_SECTION;
  } else if ((d_quote = sscanf (line, "%[^=] = \"%[^\n]\"", key, value)) == 2
         ||  sscanf (line, "%[^=] = '%[^\n]'",   key, value) == 2) {
    /* Usual key=value with quotes, with or without comments */
    strstrip(key);
    strlwc(key, key, len);
    if (d_quote == 2)
      parse_quoted_value(value, '"');
    else
      parse_quoted_value(value, '\'');
    /* Don't strip spaces from values surrounded with quotes */
    sta = LINE_VALUE;
  } else if (sscanf (line, "%[^=] = %[^;#]", key, value) == 2) {
    /* Usual key=value without quotes, with or without comments */
    strstrip(key);
    strlwc(key, key, len);
    strstrip(value);
    /*
     * sscanf cannot handle '' or "" as empty values
     * this is done here
     */
    if (!strcmp(value, "\"\"") || (!strcmp(value, "''"))) value[0]=0;
    sta = LINE_VALUE;
  } else if (sscanf(line, "%[^=] = %[;#]", key, value) == 2
       ||  sscanf(line, "%[^=] %[=]", key, value) == 2) {
    /*
     * Special cases:
     * key=
     * key=;
     * key=#
     */
    strstrip(key);
    strlwc(key, key, len);
    value[0]=0;
    sta = LINE_VALUE;
  } else {
    /* Generate syntax error */
    sta = LINE_ERROR;
  }
  xfree(line);
  return sta;
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Parse an ini file and return an allocated dictionary object
  @param  in File to read.
  @param  ininame Name of the ini file to read (only used for nicer error messages)
  @return   Pointer to newly allocated dictionary

  This is the parser for ini files. This function is called, providing
  the file to be read. It returns a dictionary object that should not
  be accessed directly, but through accessor functions instead.

  The returned dictionary must be freed using iniparser_freedict().
*/
dictionary *iniparser_load_file (dictionary *dict, FILE *in, const char *ininame, int xfreeme) {
  char line [ASCIILINESZ + 1], section[ASCIILINESZ + 1], key[ASCIILINESZ + 1],
       tmp[(ASCIILINESZ * 2) + 2], val[ASCIILINESZ + 1];
  int last, lineno, errs, mem_err, len;
  last = lineno = errs = mem_err = 0;
  if (dict == NULL) dict = dictionary_new(0);
  if (!dict) return NULL;
  tools_bzero(line, ASCIILINESZ);
  tools_bzero(section, ASCIILINESZ);
  tools_bzero(key, ASCIILINESZ);
  tools_bzero(val, ASCIILINESZ);
  last = 0;
  while (fgets(line + last, ASCIILINESZ - last, in) != NULL) {
    lineno++;
    len = (int)tools_strlen(line) - 1;
    if (len <= 0) continue;
    /* Safety check against buffer overflows */
    if (line[len] != '\n' && !feof(in)) {
      iniparser_error_callback("iniparser: input line too long in %s (%d)\n",
        ininame, lineno);
      dictionary_del(dict, xfreeme);
      return NULL;
    }
    /* Get rid of \n and spaces at end of line */
    while ((len >= 0) && ((line[len] == '\n') || (isspace(line[len])))) {
      line[len] = 0;
      len--;
    }
    if (len < 0) len = 0; /* Line was entirely \n and/or spaces */
    /* Detect multi-line */
    if (line[len] == '\\') {
      /* Multi-line value */
      last = len;
      continue;
    } else {
      last = 0;
    }
    switch (iniparser_line(line, section, key, val)) {
      case LINE_EMPTY: case LINE_COMMENT:
        break;
      case LINE_SECTION:
        mem_err = dictionary_set(dict, section, NULL);
        break;
      case LINE_VALUE:
        sprintf(tmp, "%s:%s", section, key);
        mem_err = dictionary_set(dict, tmp, val);
        break;
      case LINE_ERROR:
        iniparser_error_callback(
          "iniparser: syntax error in %s (%d):\n-> %s\n", ininame, lineno, line);
        errs++;
      break;
        default:
        break;
    }
    tools_bzero(line, ASCIILINESZ);
    last = 0;
    if (mem_err < 0) {
      iniparser_error_callback("iniparser: memory allocation failure\n");
      break;
    }
  }
  if (errs) {
    dictionary_del(dict, xfreeme);
    dict = NULL;
  }
  return dict;
}

/*-------------------------------------------------------------------------*/
/**
  @brief  Parse an ini file and return an allocated dictionary object
  @param  ininame Name of the ini file to read.
  @return   Pointer to newly allocated dictionary

  This is the parser for ini files. This function is called, providing
  the name of the file to be read. It returns a dictionary object that
  should not be accessed directly, but through accessor functions
  instead.

  The returned dictionary must be freed using iniparser_freedict().
*/
dictionary *iniparser_load (dictionary *dict, const char *ininame, int xfreeme) {
  FILE *in;
  if ((in=fopen(ininame, "r")) == NULL) {
    iniparser_error_callback("iniparser: cannot open %s\n", ininame);
    return NULL;
  }
  dict = iniparser_load_file(dict, in, ininame, xfreeme);
  fclose(in);
  return dict;
}


/*-------------------------------------------------------------------------*/
/**
  @brief  Free all memory associated to an ini dictionary
  @param  d Dictionary to free
  @return   void

  Free all memory associated to an ini dictionary.
  It is mandatory to call this function before the dictionary object
  gets out of the current context.
*/
void iniparser_freedict (dictionary *d) {
  dictionary_del(d, 1);
}


/* Here comes the Agena section ******************************************** */

#define checkdict(L, n) (dictionary *)luaL_checkudata(L, n, "ini")

/* Instantiates and returns a new dictionary structure as a Lua userdata.

   See also: ini.close, ini.read. 4.6.6/7/8 */
static int ini_new (lua_State *L) {
  dictionary *d;
  size_t bucketslots = (size_t)agnL_optposint(L, 1, BUCKETSLOTS);
  d = (dictionary *)lua_newuserdata(L, sizeof(dictionary));
  if (d == NULL) {
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "ini.new");
  } else {
    size_t size, i;
    /* If no size was specified, allocate space for DICTMINSZ */
    if (!tools_ismultiplel(bucketslots, BUCKETSLOTS, 0, DBL_EPSILON))
      luaL_error(L, "Error in " LUA_QS ": argument must be a multiple of 4.", "ini.new");
    tools_bzero(d, sizeof *d);
    d->bucketslots = bucketslots;
    size = 32*bucketslots;
    d->size = size;
    d->val  = (char **)calloc(size, sizeof *d->val);
    d->key  = (char **)calloc(size, sizeof *d->key);
    d->hash = (unsigned *)calloc(size, sizeof *d->hash);
    d->nextfree = (unsigned *)calloc(bucketslots, sizeof *d->nextfree);  /* 4.6.9 fix */
    if (!d->size || !d->val || !d->hash || !d->nextfree) {
      /* free((void *)d->size); */  /* you can't free an atomic value */
      free((void *)d->val);
      free((void *)d->hash);
      free((void *)d->nextfree);
      /* NEVER free d itself, it will break GC */
    }
    for (i=0; i < bucketslots; i++) d->nextfree[i] = i;
  }
  lua_setmetatabletoobject(L, -1, AGENA_INILIBNAME, 1);
  return 1;
}


/* Reads all the data in ini file inifile into dictionary d. The function returns nothing.

   See also: ini.close, ini.new, ini.getsection. 4.6.6 */
static int ini_read (lua_State *L) {
  dictionary *d = checkdict(L, 1);
  const char *filename = agn_checkstring(L, 2);
  iniparser_load(d, filename, 0);
  return 0;
}


static void aux_checkoptions (lua_State *L, int pos, int *nargs,
  int *comma, int *convert, int *raw, const char *procname) {
  /* we do not set eps here since the functions using this aux function need different values */
  *comma = *raw = 0;
  *convert = 1;
  int checkoptions = 3;  /* check n options; CHANGE THIS if you add/delete options */
  if (*nargs >= pos && lua_ispair(L, *nargs))  /* 6.7.8 change */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq(option, "convert")) {
        *convert = agn_checkboolean(L, -1);
      } else if (tools_streq(option, "comma")) {
        *comma = agn_checkboolean(L, -1);
      } else if (tools_streq(option, "raw")) {
        *raw = agn_checkboolean(L, -1);
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
}

/* Pushes the original or converted value onto the stack */
static FORCE_INLINE void aux_pushstringorelse (lua_State *L, char *val, int comma, int convert) {
  lua_Number num;
  int result, overflow, rc;
  rc = 1;
  if (comma)  /* convert a string representing a number with a decimal comma to a number with a decimal dot */
    tools_commatodot(val, &result);
  if (convert) {
    if (luaO_str2d(val, &num, &overflow)) { /* convert number */
      lua_pushnumber(L, num); rc = 0;
    } else if (tools_streq(val, "true")) {  /* process Booleans */
      lua_pushtrue(L); rc = 0;
    } else if (tools_streq(val, "false")) {
      lua_pushfalse(L); rc = 0;
    } else if (tools_streq(val, "fail")) {
      lua_pushfail(L); rc = 0;
    }
  }
  if (rc) lua_pushstring(L, val);
}


static int ini_dump (lua_State *L) {  /* 4.6.6/4.6.7 */
  size_t i;
  int comma, convert, overflow, raw, nargs;
  nargs = lua_gettop(L);
  aux_checkoptions(L, 2, &nargs, &comma, &convert, &raw, "ini.dump");
  dictionary *d = checkdict(L, 1);
  luaL_checkstack(L, 3 - raw, "not enough stack space");
  if (d == NULL) {
    luaL_error(L, "Error in " LUA_QS ": dictionary is empty.", "ini.dump");
  }
  if (d->n < 1) {
    lua_createtable(L, 0, 0);
    return 1;
  }
  if (raw) {
    int c;
    lua_Number num;
    char *key;
    agn_createseq(L, d->size);
    for (i=0, c=0; i < d->size; i++) {
      key = d->key[i];
      if (key) {
        if (d->val[i]) {
          if (convert && luaO_str2d(d->val[i], &num, &overflow)) {
            agn_createpairstringnumber(L, key, num);  /* 4.6.7 change */
          } else {
            agn_createpairstrings(L, key, d->val[i]);
          }
        } else {
          lua_pushstring(L, key);
        }
        lua_seqrawseti(L, -2, ++c);
      }
    }
  } else {  /* 4.6.7 extension: put all into a nice table */
    int pos, flag, type;
    const char *key, *val;
    char *res;
    lua_createtable(L, 0, 8);
    for (i=0; i < d->size; i++) {
      key = d->key[i];
      val = d->val[i];
      if (!key) continue;
      if (key && !val) {  /* we got a new section name but we make sure no subtable has been created yet */
        lua_pushstring(L, key);
        lua_rawget(L, -2);
        type = lua_type(L, -1);
        agn_poptop(L);
        switch (type) {
          case LUA_TNIL:
            lua_pushstring(L, key);
            lua_createtable(L, 0, 8);
            lua_rawset(L, -3);
            break;
          case LUA_TTABLE:
            break;
          default:
            luaL_error(L, "Error in " LUA_QS ": something went wrong.", "ini.dump");
        }
        continue;
      }
      /* we have a key~value pair, structure: '<sectionname>:<keyname>' */
      flag = 0;
      if ((res = strchr(key, ':')) == NULL) {
        agn_poptop(L);  /* drop table */
        luaL_error(L, "Error in " LUA_QS ": key " LUA_QS " is malformed.", "ini.dump", key);
      }
      pos = res - key;
      if (pos == 0) {  /* key-value pair without association to a section (<:taxi>:<Pizza Cab>: assign to table root */
        lua_pushstring(L, key + 1);
        aux_pushstringorelse(L, (char *)val, comma, convert);
        lua_rawset(L, -3);
        continue;
      }
      lua_pushlstring(L, key, pos);  /* push section name */
      lua_rawget(L, -2);  /* get associated table */
      if (lua_isnil(L, -1)) {  /* there is no section yet, so create one */
        agn_poptop(L);
        lua_createtable(L, 0, 8);
        flag = 1;  /* put new table into result table later */
      }
      if (!lua_istable(L, -1)) {
        agn_poptop(L);
        luaL_error(L, "Error in " LUA_QS ": something went wrong.", "ini.dump");
      }
      /* now there is a section table on the stack top, put key~value pair into it */
      lua_pushstring(L, key + pos + 1);
      aux_pushstringorelse(L, (char *)val, comma, convert);
      lua_rawset(L, -3);
      /* subtable is still on top */
      if (flag) {  /* we created a new subtable before, we must store it in result table */
        lua_pushlstring(L, key, pos);
        lua_insert(L, -2);
        lua_rawset(L, -3);
      } else {
        agn_poptop(L);
      }
    }
    /* return table */
  }
  return 1;
}


/* Retrieves all the key-value pairs from dictionary `d` for the given section `n` or `str`. You can either pass the
   section as a non-negative number `n` or a string `str`.

   The return is a table.

   If `n` is 0 or `str` is the empty string, then all key-value pairs that are not in any section, if they exist at all,
   are returned.

   If section number `n` or section name `str` do not exist or the dictionary is empty, the function returns `fail`.

   There are the following options:

   convert = false: Do not automatically convert strings representing numbers to Agena numbers. Default is `true`.
   comma = true:    Convert a decimal comma in a string representing a number to a decimal dot. Default is `false`.

   With the ini file 'sample.ini':

   ; Pizza general
   Taxi=Pizza Cab
   State=

   ; Following is a section ...
   [Pizza]
   ; ... and now the associated key~value pairs
   Ham = yes
   Mushrooms = true
   Capres = 0
   Cheese = "Non" ;
   Gravy=false
   Price = 3.99
   Comment= This \
   is a \
   multiline string.
   Preis=3,99
   empty =

   ; end of ini file

   a typical usage might look like this:

   > d := ini.new()  # instantiate the dictionary

   > ini.read(d, 'sample.ini')  # fill it with data of an ini file

   > ini.getsection(d, 'Pizza', comma = true):  # get key-value pairs in section 'Pizza'
   [capres ~ 0, cheese ~ Non, comment ~ This is a multiline string., empty ~ , gravy ~ false, ham ~ yes, mushrooms ~ true, preis ~ 3.99, price ~ 3.99]

   > ini.getsection(d, 0):  # get all key-value pairs that are not in any section
   [state ~ , taxi ~ Pizza Cab]

   > d@@attrib():  # get info on the dictionary, `sections` denotes the number of sections
   [allocated ~ 128, assigned ~ 22, sections ~ 1]

   > d@@close();  # drop the dictionary, alternatively execute: ini.close(d)

   See also: utils.readini. 4.6.6
*/
static int ini_getsection (lua_State *L) {
  int nargs, comma, convert, raw;
  char keym[ASCIILINESZ + 1];
  size_t i;
  nargs = lua_gettop(L);
  aux_checkoptions(L, 2 + (nargs != 1), &nargs, &comma, &convert, &raw, "ini.getsection");
  (void)raw;
  dictionary *d = checkdict(L, 1);
  if (d == NULL) {
    luaL_error(L, "Error in " LUA_QS ": userdata is invalid.", "ini.getsection");
    return 0;
  }
  if (d->n == 0) {
    lua_pushfail(L);
    return 1;
  }
  if (nargs == 1) {  /* get section names only */
    int c = 0;
    lua_createtable(L, 8, 0);
    for (i=0; i < d->size; i++) {
      if (d->key[i] && !d->val[i]) {
        lua_pushstring(L, d->key[i]);
        lua_rawseti(L, -2, ++c);
      }
    }
  } else {
    int pos;
    const char *s;
    size_t l;
    if (agn_isinteger(L, 2)) {  /* section number given ? */
      int no = agn_tointeger(L, 2);
      if (no > 0) {
        s = iniparser_getsecname(d, no - 1);  /* indices start from 1, but we must subtract 1 */
        if (s == NULL) {
          lua_pushfail(L);
          return 1;
        }
      } else {  /* get key-value pairs that are not in any section */
        s = ""; l = 0;
      }
      l = tools_strlen(s);
    } else {  /* section name given */
      s = agn_checklstring(L, 2, &l);
    }
    if (l && !iniparser_find_entry(d, s)) {  /* section does not exist. We will treat the `argument-is-the-empty-string` situation below. */
      lua_pushfail(L);
      return 1;
    }
    strlwc(s, keym, sizeof(keym));  /* convert section name to lower case and store in keym */
    keym[l] = ':';  /* mark end of section name with a colon */
    luaL_checkstack(L, 2, "not enough stack space");
    lua_createtable(L, 0, 8);
    for (i=0; i < d->size; i++) {
      char *key = d->key[i];
      char *val = d->val[i];
      if (key) {
        pos = tools_strncmp(key, keym, l + 1);  /* key in dictionary starts with the given section name (pos == 0, e.g. 'section1:key1') ? */
        if (!pos && val) {  /* we have a hit, that is there is a key-value pair for the section */
          while (*key++ != ':');  /* skip section part */
          lua_pushstring(L, key); /* push key name of pair */
          aux_pushstringorelse(L, val, comma, convert);  /* push value of pair, 4.6.7 change */
          lua_rawset(L, -3);
        }
      }
    }
  }
  return 1;
}


/* Empties and discards dictionary d, giving back memory to the interpreter. The function returns nothing.

   See also: ini.new. 4.6.6 */
static int ini_close (lua_State *L) {
  dictionary *d = checkdict(L, 1);
  if (d == NULL)
    luaL_error(L, "Error in " LUA_QS ": dictionary has already been closed.", "ini.close");
  dictionary_del(d, 0);
  lua_setmetatabletoobject(L, 1, NULL, 1);
  return 0;
}


/* Returns information on dictionary d. The result is a table with the following key~value pairs:
   'assigned' ~ <an integer>: number of assigned slots,
   'allocated' ~ <an integer>: number of allocated slots´,
   'sections' ~ <an integer>: number of sections. 4.6.6 */
static int ini_attrib (lua_State *L) {
  int i;
  dictionary *d = checkdict(L, 1);
  luaL_checkstack(L, 3, "not enough stack space");
  lua_createtable(L, 0, 3);
  lua_pushstring(L, "assigned");
  lua_pushnumber(L, d->n);     /* number of assigned slots */
  lua_rawset(L, -3);
  lua_pushstring(L, "allocated");
  lua_pushnumber(L, d->size);  /* number of allocated slots */
  lua_rawset(L, -3);
  lua_pushstring(L, "sections");
  lua_pushinteger(L, iniparser_getnsec(d));  /* number of sections */
  lua_rawset(L, -3);
  lua_pushstring(L, "bucketslots");  /* 4.6.8 */
  lua_pushinteger(L, d->bucketslots);
  lua_rawset(L, -3);
  lua_pushstring(L, "nextfree");  /* new 4.6.8 */
  lua_createtable(L, d->bucketslots, 0);
  for (i=0; i < d->bucketslots; i++) lua_rawsetinumber(L, -1, i + 1, d->nextfree[i]);
  lua_rawset(L, -3);
  return 1;
}


/* Computes the hash used internally by the ini package for string `str` and returns an integer. 4.6.7 */
static int ini_hash (lua_State *L) {
  uint32_t h, n;
  const char *str = agn_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);
  h = dictionary_hash(str);
  lua_pushnumber(L, (n != 0) ? h % n : h);
  return 1;
}


/* The function locates section or key `item` in dictionary d and in case of a key-value pair returns the
   associated value, in case of a section name returns `null` and if `item` could not be found at all
   explicitly issues an error. The function automatically converts `item` to lower case before searching
   the dictionary to avoid failures. 4.6.7 */
#define NOTFOUND "[xUN-DE-FI-NEDx]"
static int ini_getitem (lua_State *L) {
  int rc, run, nargs, comma, convert, raw;
  char *item;
  const char *str0, *str1, *r;
  char tmp_str[ASCIILINESZ + 1];
  dictionary *d = checkdict(L, 1);
  str0 = luaL_checkstring(L, 2);
  nargs = 2;
  str1 = NULL;
  if (lua_isstring(L, 3)) {
    nargs++;
    str1 = luaL_checkstring(L, 3);
  }
  nargs++;
  aux_checkoptions(L, nargs, &nargs, &comma, &convert, &raw, "ini.getitem");
  item = str_concat(str0, str1 ? ":" : NULL, str1, NULL);
  run = 0;
getitem_reentry:
  if (!item) {
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "ini.getitem");
  }
  r = dictionary_get(d, strlwc(item, tmp_str, sizeof(tmp_str)), NOTFOUND, &rc);
  if (rc == -1) {
    xfree(item);
    if (d->n == 0) {
      luaL_error(L, "Error in " LUA_QS ": dictionary is empty.", "ini.getitem");
    } else if (!str1 && !run) {
      item = str_concat(":", str0, NULL);
      run = 1;
      goto getitem_reentry;
    } else {
      luaL_error(L, "Error in " LUA_QS ": item could not be found.", "ini.getitem");
    }
  }
  if (rc) { /* we have a key-value pair, probably convert it, 4.6.8 */
    aux_pushstringorelse(L, (char *)r, comma, convert);
  } else  /* we have a section name */
    lua_pushnil(L);
  xfree(item);
  return 1;
}


/* The function sets data into a dictionary. 4.6.7 */
static int ini_setitem (lua_State *L) {
  char *t;
  const char *item, *val;
  char tmp_str[ASCIILINESZ + 1];
  int nargs = lua_gettop(L);
  dictionary *d = checkdict(L, 1);
  item = luaL_checkstring(L, 2);
  if (nargs < 4) {
    val = luaL_optstring(L, 3, NULL);
    t = str_concat(val && strchr(item, ':') == NULL ? ":" : "", item, NULL);
  } else {
    const char *key = agn_checkstring(L, 3);
    val = luaL_optstring(L, 4, NULL);
    t = str_concat(item, ":", key, NULL);
  }
  if (!t)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "ini.setitem");
  lua_pushboolean(L, dictionary_set(d, strlwc(t, tmp_str, sizeof(tmp_str)), val) == 0);
  xfree(t);
  return 1;
}


/* The function deletes data from a dictionary. 4.6.7 */
static int ini_unset (lua_State *L) {
  char *t;
  const char *item, *key;
  char tmp_str[ASCIILINESZ + 1];
  size_t l, rc;
  dictionary *d = checkdict(L, 1);
  item = agn_checklstring(L, 2, &l);
  key = luaL_optstring(L, 3, NULL);
  rc = l && !iniparser_find_entry(d, item);  /* item is not a section name */
  t = str_concat(rc ? ":" : "", item, key ? ":" : NULL, key, NULL);
  if (!t)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "ini.unset");
  lua_pushboolean(L, dictionary_unset(d, strlwc(t, tmp_str, sizeof(tmp_str))) == 0);
  xfree(t);
  return 1;
}


static int mt_inigc (lua_State *L) {
  dictionary *d = checkdict(L, 1);
  if (d == NULL) return 0;
  dictionary_del(d, 0);
  lua_setmetatabletoobject(L, 1, NULL, 1);
  return 0;
}


static const struct luaL_Reg ini_lib [] = {  /* metamethods for ini userdata `n` */
  {"attrib", ini_attrib},          /* added on December 25, 2024 */
  {"close", ini_close},            /* added on December 25, 2024 */
  {"dump", ini_dump},              /* added on December 25, 2024 */
  {"getitem", ini_getitem},        /* added on December 27, 2024 */
  {"getsection", ini_getsection},  /* added on December 25, 2024 */
  {"hash", ini_hash},              /* added on December 27, 2024 */
  {"read", ini_read},              /* added on December 25, 2024 */
  {"setitem", ini_setitem},        /* added on December 27, 2024 */
  {"unset", ini_unset},            /* added on December 27, 2024 */
  {"__gc", mt_inigc},              /* do not forget garbage collection */
  {NULL, NULL}
};

static const luaL_Reg inilib[] = {
  {"attrib", ini_attrib},          /* added on December 25, 2024 */
  {"close", ini_close},            /* added on December 25, 2024 */
  {"dump", ini_dump},              /* added on December 25, 2024 */
  {"getitem", ini_getitem},        /* added on December 27, 2024 */
  {"getsection", ini_getsection},  /* added on December 25, 2024 */
  {"hash", ini_hash},              /* added on December 27, 2024 */
  {"new", ini_new},                /* added on December 25, 2024 */
  {"read", ini_read},              /* added on December 25, 2024 */
  {"setitem", ini_setitem},        /* added on December 27, 2024 */
  {"unset", ini_unset},            /* added on December 27, 2024 */
  {NULL, NULL}
};


/*
** Open ini library
*/

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, AGENA_INILIBNAME);  /* create metatable */
  lua_pushvalue(L, -1);                    /* push metatable */
  lua_setfield(L, -2, "__index");          /* metatable.__index = metatable */
  luaL_register(L, NULL, ini_lib);         /* methods */
}

LUALIB_API int luaopen_ini (lua_State *L) {
  /* metamethods */
  createmeta(L);
  lua_pop(L, 1);  /* 7.11.2 fix */
  /* register library */
  luaL_register(L, AGENA_INILIBNAME, inilib);
  return 1;
}

