/*
** $Id: phones.c, 25/10/2026 alex Exp $
** phones library for phonetics
** See Copyright Notice in agena.h
*/

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define phoneslib_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agncmpt.h"
#include "agnconf.h"  /* for uchar */
#include "agnxlib.h"


#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_PHONESLIBNAME "phones"
LUALIB_API int (luaopen_open) (lua_State *L);
#endif


/* phonetiQs **********************************************************************************/

/* Table of characters: decimal, hex, character
  65, 41 a  66, 42 b  67, 43 c  68, 44 d  69, 45 e  70, 46 f  71, 47 g
  72, 48 h  73, 49 i  74, 4A j  75, 4B k  76, 4C l  77, 4D m  78, 4E n
  79, 4F o  80, 50 p  81, 51 q  82, 52 r  83, 53 s  84, 54 t  85, 55 u
  86, 56 v  87, 57 w  88, 58 x  89, 59 y  90, 5A z */

static unsigned char phQmap[256] = {
   0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,  /* 0 .. 7*/
/* 00NUL 01SOH 02STX 03ETX 04EOT 05ENQ 06ACK 07BEL */
   0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,  /* 8 .. 15 */
/* 08BS  09HT  10LF  11VT  12FF  13CR  14SO  15SI */
   0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,  /* 16 .. 23 */
/* 16DLE 17DC1 18DC2 19DC3 20DC4 21NAK 22SYN 23ETB */
   0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,  /* 24 .. 31 */
/* 24CAN 25EM  26SUB 27ESC 28FS  29GS  30RS  31US */
   0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,  /* 32 .. 39 */
/* 32    33!   34"   35#   36$   37%   38&   39' */
   0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,  /* 40 .. 47 */
/* 40(   41)   42*   43+   44,   45-   46.   47/ */
   0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,  /* 48 .. 55 */
/* 480   491   502   513   524   535   546   557 */
   0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,  /* 56 .. 63 */
/* 568   579   58:   59;   60<   61=   62>   63? */
   0x40, 0x23, 0x42, 0x43, 0x44, 0x23, 0x46, 0x47,  /* 64 .. 71 */
/* 64@   65A   66B   67C   68D   69E   70F   71G */
   0x23, 0x23, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x23,  /* 72 .. 79 */
/* 72H   73I   74J   75K   76L   77M   78N   79O */
   0x50, 0x51, 0x52, 0x53, 0x54, 0x23, 0x56, 0x57,  /* 80 .. 87 */
/* 80P   81Q   82R   83S   84T   85U   86V   87W */
   0x58, 0x23, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,  /* 88 .. 95 */
/* 88X   89Y   90Z   91[   92\   93]   94^   95_ */
   0x60, 0x23, 0x50, 0x4b, 0x54, 0x23, 0x46, 0x4b,  /* 96 .. 103 */
/* 096`  097a  098b  099c  100d  101e  102f  103g */
   0x23, 0x23, 0x4a, 0x4b, 0x4c, 0x4e, 0x4e, 0x23,  /* 104 .. 111 */
/* 104h  105i  106j  107k  108l  109m  110n  111o */
   0x50, 0x4b, 0x52, 0x53, 0x54, 0x23, 0x46, 0x46,  /* 112 .. 119 */
/* 112p  113q  114r  115s  116t  117u  118v  119w */
   0x53, 0x23, 0x53, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,  /* 120 .. 127 */
/* 120x  121y  122z  123{  124|  125}  126~  127¦ */
   0x40, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x40,  /* 128 .. 135 */
/* 128Ç  129ü  130é  131â  132ä  133à  134å  135ç */
   0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,  /* 136 .. 143 */
/* 136ê  137ë  138è  139ï  140î  141ì  142Ä  143Å */
   0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,  /* 144 .. 151 */
/* 144É  145æ  146Æ  147ô  148ö  149ò  150û  151ù */
   0x23, 0x23, 0x23, 0x23, 0x9c, 0x9d, 0x9e, 0x9f,  /* 152 .. 159 */
/* 152ÿ  153Ö  154Ü  155ø  156£  157Ø  158×  159ƒ */
   0x23, 0x23, 0x23, 0x23, 0xa4, 0xa5, 0xa6, 0xa7,  /* 160 .. 167 */
/* 160á  161í  162ó  163ú  164ñ  165Ñ  166ª  167º */
   0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,  /* 168 .. 175 */
/* 168¿  169®  170¬  171½  172¼  173¡  174«  175» */
   0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0x23, 0x23, 0x23,  /* 176 .. 183 */
/* 176¦  177¦  178¦  179¦  180¦  181Á  182Â  183À */
   0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,  /* 184 .. 191 */
/* 184©  185¦  186¦  187+  188+  189¢  190¥  191+ */
   0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0x23, 0x23,  /* 192 .. 199 */
/* 192+  193-  194-  195+  196-  197+  198ã  199Ã */
   0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,  /* 200 .. 207 */
/* 200+  201+  202-  203-  204¦  205-  206+  207¤ */
   0xe8, 0xe8, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,  /* 208 .. 215 */
/* 208ð  209Ð  210Ê  211Ë  212È  213i  214Í  215Î */
   0x23, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0x23, 0xdf,  /* 216 .. 223 */
/* 216Ï  217+  218+  219¦  220_  221¦  222Ì  223¯ */
   0x23, 0x53, 0x23, 0x23, 0x23, 0x23, 0xe6, 0xe8,  /* 224 .. 231 */
/* 224Ó  225ß  226Ô  227Ò  228õ  229Õ  230µ  231þ */
   0xe8, 0x23, 0x23, 0x23, 0x23, 0x23, 0xee, 0xef,  /* 232 .. 239 */
/* 232Þ  233Ú  234Û  235Ù  236ý  237Ý  238¯  239´ */
   0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,  /* 240 .. 247 */
/* 240­  241±  242=  243¾  244¶  245§  246÷  247¸ */
   0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff   /* 248 .. 255 */
/* 248°  249¨  250·  251¹  252³  253²  254¦  255 */
};

static unsigned char vowelsign = '#';

static unsigned char sdxMap[256] = {
   0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,  /* 0 .. 7 */
/* 00NUL 01SOH 02STX 03ETX 04EOT 05ENQ 06ACK 07BEL */
   0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,  /* 8 .. 15 */
/* 08BS  09HT  10LF  11VT  12FF  13CR  14SO  15SI */
   0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,  /* 16 .. 23 */
/* 16DLE 17DC1 18DC2 19DC3 20DC4 21NAK 22SYN 23ETB */
   0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,  /* 24 .. 31 */
/* 24CAN 25EM  26SUB 27ESC 28FS  29GS  30RS  31US */
   0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,  /* 32 .. 39 */
/* 32    33!   34"   35#   36$   37%   38&   39' */
   0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,  /* 40 .. 47 */
/* 40(   41)   42*   43+   44,   45-   46.   47/ */
   0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,  /* 48 .. 55 */
/* 480   491   502   513   524   535   546   557 */
   0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,  /* 56 .. 63 */
/* 568   579   58:   59;   60<   61=   62>   63? */
   0x40,  '0',  '1',  '2',  '3',  '0',  '1',  '2',  /* 64 .. 71 */
/* 64@   65A   66B   67C   68D   69E   70F   71G */
    '.',  '0',  '2',  '2',  '4',  '5',  '5',  '0',  /* 72 .. 79 */
/* 72H   73I   74J   75K   76L   77M   78N   79O */
    '1',  '2',  '6',  '2',  '3',  '0',  '1',  '.',  /* 80 .. 87 */
/* 80P   81Q   82R   83S   84T   85U   86V   87W */
    '2',  '0',  '2', 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,  /* 88 .. 95 */
/* 88X   89Y   90Z   91[   92\   93]   94^   95_ */
   0x60,  '0',  '1',  '2',  '3',  '0',  '1',  '2',  /* 96 .. 103 */
/* 96`   97a   98b   99c  100d  101e  102f  103g */
    '.',  '0',  '2',  '2',  '4',  '5',  '5',  '0',  /* 104 .. 111 */
/* 104h  105i  106j  107k  108l  109m  110n  111o */
    '1',  '2',  '6',  '2',  '3',  '0',  '1',  '.',  /* 112 .. 119 */
/* 112p  113q  114r  115s  116t  117u  118v  119w */
    '2',  '0',  '2', 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,  /* 120 .. 127 */
/* 120x  121y  122z  123{  124|  125}  126~  127¦ */
   0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,  /* 128 .. 135 */
/* 128Ç  129ü  130é  131â  132ä  133à  134å  135ç */
   0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,  /* 136 .. 143 */
/* 136ê  137ë  138è  139ï  140î  141ì  142Ä  143Å */
   0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,  /* 144 .. 151 */
/* 144É  145æ  146Æ  147ô  148ö  149ò  150û  151ù */
   0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,  /* 152 .. 159 */
/* 152ÿ  153Ö  154Ü  155ø  156£  157Ø  158×  159ƒ */
   0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,  /* 160 .. 167 */
/* 160á  161í  162ó  163ú  164ñ  165Ñ  166ª  167º */
   0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,  /* 168 .. 175 */
/* 168¿  169®  170¬  171½  172¼  173¡  174«  175» */
   0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,  /* 176 .. 183 */
/* 176¦  177¦  178¦  179¦  180¦  181Á  182Â  183À */
   0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,  /* 184 .. 191 */
/* 184©  185¦  186¦  187+  188+  189¢  190¥  191+ */
   0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,  /* 192 .. 199 */
/* 192+  193-  194-  195+  196-  197+  198ã  199Ã */
   0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,  /* 200 .. 207 */
/* 200+  201+  202-  203-  204¦  205-  206+  207¤ */
   0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,  /* 208 .. 215 */
/* 208ð  209Ð  210Ê  211Ë  212È  213i  214Í  215Î */
   0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,  /* 216 .. 223 */
/* 216Ï  217+  218+  219¦  220_  221¦  222Ì  223¯ */
   0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,  /* 224 .. 231 */
/* 224Ó  225ß  226Ô  227Ò  228õ  229Õ  230µ  231þ */
   0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,  /* 232 .. 239 */
/* 232Þ  233Ú  234Û  235Ù  236ý  237Ý  238¯  239´ */
   0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,  /* 240 .. 247 */
/* 240­  241±  242=  243¾  244¶  245§  246÷  247¸ */
   0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff   /* 248 .. 255 */
/* 248°  249¨  250·  251¹  252³  253²  254¦  255  */
};

#ifdef DONOTCOMPILE
/* Do not remap characters > ASCII code 127 */
static unsigned char phQmap7bit[256] = {
   0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,  /* 0 .. 7*/
   0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,  /* 8 .. 15 */
   0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,  /* 16 .. 23 */
   0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,  /* 24 .. 31 */
   0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,  /* 32 .. 39 */
   0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,  /* 40 .. 47 */
   0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,  /* 48 .. 55 */
   0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,  /* 56 .. 63 */
   0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,  /* 64 .. 71 */
   0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,  /* 72 .. 79 */
   0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,  /* 80 .. 87 */
   0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,  /* 88 .. 95 */
   0x60, 0x23, 0x50, 0x4b, 0x54, 0x23, 0x46, 0x4b,  /* 96 .. 103 */
   0x23, 0x23, 0x4a, 0x4b, 0x4c, 0x4e, 0x4e, 0x23,  /* 104 .. 111 */
   0x50, 0x4b, 0x52, 0x53, 0x54, 0x23, 0x46, 0x46,  /* 112 .. 119 */
   0x53, 0x23, 0x53, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,  /* 120 .. 127 */
   0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,  /* 128 .. 135 */
   0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x0e, 0x8f,  /* 136 .. 143 */
   0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,  /* 144 .. 151 */
   0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,  /* 152 .. 159 */
   0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,  /* 160 .. 167 */
   0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,  /* 168 .. 175 */
   0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,  /* 176 .. 183 */
   0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,  /* 184 .. 191 */
   0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,  /* 192 .. 199 */
   0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,  /* 200 .. 207 */
   0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,  /* 208 .. 215 */
   0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,  /* 216 .. 223 */
   0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,  /* 224 .. 231 */
   0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,  /* 232 .. 239 */
   0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,  /* 240 .. 247 */
   0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff   /* 248 .. 255 */
};
#endif

/* Computes a phonetic value of the input string by deleting neighbouring equal phonemes only
   and optionally preserving vowels within the word, by default not capping after a number of
   resulting characters:

      mueller -> M#L#R

   The phonetic mapping is not compatible to Soundex since w is regarded a consonant
   and there is a difference between 'j' and 'c', 'k', 'q', 'g' and 's', 'z', 'x' as well as
   between 'b', 'p' and 'f', 'v'.

   'a', 'e', 'i', 'o', 'u', 'y', 'h', diacritics -> '#'
   'b', 'p'                                      -> 'P'
   'f', 'v', 'w'                                 -> 'F'
   'd', 't'                                      -> 'T'
   'm', 'n'                                      -> 'N'
   'l'                                           -> 'L'
   'r'                                           -> 'R'
   'j'                                           -> 'J'
   'c', 'k', 'q', 'g'                            -> 'K'
   's', 'z', 'x'                                 -> 'S'

   Optimized Single-Loop by Gemini AI, twice as fast, 7.11.6


Names with Letters Side-by-Side that have the Same Code Number:
---------------------------------------------------------------

If the surname has different letters side-by-side that have the same number in
the coding guide, they should be treated as one letter.

Do the same if any adjacent double letters are equal.

Examples:

Soundex & phoentiQs: both YES

Pfister is coded as P-236 (P, F ignored, 2 for the S, 3 for the T, 6 for the R).

> phones.soundex('Pfister'):
P236

> phones.phonetiQs('Pfister'):
PFSTR

> phones.phonetiQs('Llamar'):
LNR

> phones.soundex('Llamar'):
L560


Consonant Separators:
---------------------

If a vowel (A, E, I, O, U) separates two consonants that have the same soundex
code, the consonant to the right of the vowel is coded.

Soundex & phoentiQs: both YES

Tymczak is coded as T-522 (T, 5 for the M, 2 for the C, Z ignored (see
"Side-by-Side" rule above), 2 for the K). Since the vowel "A" separates
the Z and K, the K is coded.

> phones.soundex('Tymzcak'):
T522

> phones.phonetiQs('Tymzcak'):
TNSKK

Soundex: YES, phonetiQs: NO

If "H" or "W" separate two consonants that have the same soundex code, the
consonant to the right of the vowel is not coded.

Ashcraft is coded A-261 (A, 2 for the S, C ignored, 6 for the R, 1 for the F).
It is not coded A-226.

> phones.soundex('Ashcraft'):
A261

> phones.phonetiQs('Ashcraft'):
#SKRFT
*/
/* 7.11.6, designed and written by Alexander Walz, tuned by Gemini AI. */
static int phones_phonetiQs (lua_State *L) {
  size_t l, i, pos;
  int maxlen, novowels;
  unsigned char curmapped, lastchar;
  const char *str;
  str = agn_checklstring(L, 1, &l);
  if (l == 0) {
    lua_pushliteral(L, "");
    return 1;
  } else if (l > 1048576) {
    luaL_error(L, "Error in " LUA_QS ": string has more than 1,048,576 bytes.", "phones.phonetiQs");
  }
  novowels = agnL_optboolean(L, 2, 1);  /* delete vowels by default */
  maxlen = agnL_optposint(L, 3, l);     /* cap after maxlen characters in _resulting_ string */
  if ((size_t)maxlen > l) maxlen = l;
  char result[l + 1];  /* allocate on the stack frame (no heap overhead) */
  /* map and store the first character unconditionally at index 0 */
  unsigned char firstmapped = phQmap[uchar(str[0])];
  result[0] = firstmapped;
  pos = 0;
  /* initialize tracking baseline with the first mapped character */
  lastchar = firstmapped;
  if (novowels) {
    for (i=1; i < l; i++) {
      curmapped = phQmap[uchar(str[i])];
      if (curmapped != vowelsign) {
        /* consonant found: only insert if it differs from the immediate prior consonant */
        if (curmapped != lastchar) {
          result[++pos] = curmapped;
          lastchar = curmapped;
        }
      } else {
        /* Vowel found: skipped in the output, but it breaks the consonant duplication chain.
           Reset lastchar to '#' (vowelsign) guarantees the next consonant will be preserved. */
        lastchar = vowelsign;
      }
    }
  } else {
    /* standard deduplication path when vowels are preserved */
    for (i=1; i < l; i++) {
      curmapped = phQmap[uchar(str[i])];
      if (curmapped != lastchar) {
        result[++pos] = curmapped;
        lastchar = curmapped;
      }
    }
  }
  size_t finallen = pos + 1;
  if (finallen > (size_t)maxlen) {
    finallen = maxlen;
  }
  lua_pushlstring(L, result, finallen);
  return 1;
}


static int phones_getphonetiQs (lua_State *L) {  /* 7.11.6, created by Gemini AI */
  return agnL_getfrommappingtable(L, 0, 1, phQmap, "phones.getphonetiQs");
}

static int phones_setphonetiQs (lua_State *L) {  /* 7.11.6, created by Gemini AI */
  return agnL_settomappingtable(L, 0, 1, phQmap, "phones.setphonetiQs");
}


static int phones_getsoundex (lua_State *L) {  /* 7.12.1 */
  return agnL_getfrommappingtable(L, 0, 1, sdxMap, "phones.getsoundex");
}

static int phones_setsoundex (lua_State *L) {  /* 7.11.6, created by Gemini AI */
  return agnL_settomappingtable(L, 0, 1, sdxMap, "phones.setsoundex");
}


/* Gets or sets the current character denoting a vowel in the output of `phones.soundex`,
   `phones.phonetiQs`, 7.11.6 */
static int phones_vowelsymbol (lua_State *L) {
  int i;
  size_t l;
  unsigned char current_vowel_sign = vowelsign;
  if (lua_gettop(L) == 0) {
    goto finale;
  } else if (agn_isstring(L, 1)) {
    const char *str = agn_checklstring(L, 1, &l);
    if (l == 1) vowelsign = str[0];
  } else if (agn_isposint(L, 1)) {
    int vowelbyte = (int)agn_tointeger(L, 1);
    if (vowelbyte < 256) vowelsign = vowelbyte;
  } else {
    return luaL_error(L, "Error in " LUA_QS
      ": expected an integer 1-255 or a non-empty character\n   for the sign.",
      "phones.vowelsymbol");
  }
  for (i=0; i < 256; i++) {
    if (phQmap[i] == current_vowel_sign) {
      phQmap[i] = vowelsign;
    }
  }
finale:
  lua_pushchar(L, vowelsign);
  return 1;
}


/* Soundex ************************************************************************************/

/* For a great easy-to.understand description of Soundex visit:

   https://www.archives.gov/research/census/soundex */

/* Helper macro to map characters to official Soundex codes.
   Returns '0' for skipped vowels/characters (A, E, I, O, U, Y).
   Returns '.' for ignored separators (H, W).
   Returns -1 for invalid non-alphabetical characters. Created by Gemini AI, 7.11.6
static char getsoundexcode (unsigned char c) {
  c = toupper(c);
  if (c < 'A' || c > 'Z') return -1;
  switch (c) {
    case 'B': case 'F': case 'P': case 'V': return '1';
    case 'C': case 'G': case 'J': case 'K': case 'Q': case 'S': case 'X': case 'Z': return '2';
    case 'D': case 'T': return '3';
    case 'L': return '4';
    case 'M': case 'N': return '5';
    case 'R': return '6';
    case 'A': case 'E': case 'I': case 'O': case 'U': case 'Y': return '0';  // Vowels
    case 'H': case 'W': return '.';  // Separators
    default: return -1;
  }
} */

/* Following the National Archives (NARA) rules, this function implements the original, official
   Soundex algorithm. It takes a string s and returns its 4-character phonetic code. 7.11.6 */
static int phones_soundex (lua_State *L) {
  size_t i, len, out_pos, start;
  const char *str = luaL_checklstring(L, 1, &len);
  /* Skip leading whitespace or locate the true first alphabetical character */
  start = 0;
  while (start < len && !isalpha(uchar(str[start]))) {
    start++;
  }
  /* Edge case: If string contains no letters, return the default empty pad */
  if (start >= len) {
    lua_pushliteral(L, "0000");
    return 1;
  }
  /* Soundex outputs are strictly formatted to 4 characters */
  char out[4] = {'0', '0', '0', '0'};
  unsigned char first_raw = str[start];
  out[0] = toupper(first_raw);
  char last_code = sdxMap[first_raw];
  out_pos = 1;
  for (i=start + 1; i < len && out_pos < 4; i++) {
    char cur_code = sdxMap[uchar(str[i])];
    /* Ignore non-alphabetical characters entirely */
    if (cur_code == -1) continue;
    if (cur_code == '.') {
      /* Rule: H and W are completely skipped.
         They do NOT reset the duplicate compression chain. */
      continue;
    } else if (cur_code == '0') {
      /* Rule: Vowels (and Y) are skipped in the output,
         but they act as a hard barrier that breaks/resets the duplicate chain. */
      last_code = '0';
    } else {
      /* Rule: Consonant found. Save it only if it represents a different
         sound group than the immediate valid phonetic baseline. */
      if (cur_code != last_code) {
        out[out_pos++] = cur_code;
        last_code = cur_code;
      }
    }
  }
  lua_pushlstring(L, out, 4);
  return 1;
}


static const luaL_Reg phoneslib[] = {
  {"getphonetiQs", phones_getphonetiQs},   /* added on October 25, 2026 */
  {"getsoundex",   phones_getsoundex},     /* added on October 28, 2026 */
  {"setphonetiQs", phones_setphonetiQs},   /* added on October 25, 2026 */
  {"setsoundex",   phones_setsoundex},     /* added on October 25, 2026 */
  {"phonetiQs",    phones_phonetiQs},      /* added on October 25, 2026 */
  {"soundex",      phones_soundex},        /* added on October 25, 2026 */
  {"vowelsymbol",  phones_vowelsymbol},    /* added on October 26, 2026 */
  {NULL, NULL}
};


/*
** Open phones library
*/
LUALIB_API int luaopen_phones (lua_State *L) {
  /* we will inject the package functions into the global strings metatable via library.agn */
  luaL_register(L, AGENA_PHONESLIBNAME, phoneslib);
  return 1;
}