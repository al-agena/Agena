/*
** $Id: lstrlib.c,v 1.132 2006/04/26 20:41:19 roberto Exp $
** Standard library for string operations and pattern-matching
** See Copyright Notice in agena.h
*/

#include <ctype.h>
#include <errno.h>  /* errno */
#include <locale.h>
#include <math.h>   /* HUGE_VAL */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define lstrlib_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agncmpt.h"
#include "agnconf.h"  /* for uchar */
#include "agnxlib.h"
#include "charbuf.h"
#include "divsufsort.h"
#include "jwent.h"
#include "ldebug.h"   /* for luaG_runerror */
#include "llimits.h"  /* for MAX_INT */
#include "lstrlib.h"
#include "lucase.def"
#include "sunpro.h"

/* see share/ascii.xls */

static const unsigned char tools_charmap[257] = {
/* Dec, HX, Chr      __UPPER|__LOWER|__DIGIT|__HEXAD|__PRINT|__PUNCT|__CNTRL|__BLANK */
/*  -1, -1, EOF */   __0____|__0____|__0____|__0____|__0____|__0____|__0____|__0____,
/* 000, 00, NUL */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 001, 01, SOH */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 002, 02, STX */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 003, 03, ETX */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 004, 04, EOT */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 005, 05, ENQ */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 006, 06, NAK */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 007, 07, BEL */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 008, 08, BS  */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 009, 09, TAB */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__BLANK,
/* 010, 0a, LF  */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__BLANK,
/* 011, 0b, VT  */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__BLANK,
/* 012, 0c, FF  */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__BLANK,
/* 013, 0d, CR  */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__BLANK,
/* 014, 0e, SI  */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 015, 0f, SO  */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 016, 10,     */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 017, 11,     */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 018, 12,     */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 019, 13,     */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 020, 14,     */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 021, 15,     */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 022, 16,     */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 023, 17,     */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 024, 18,     */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 025, 19,     */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 026, 1a,     */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 027, 1b,     */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 028, 1c,     */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 029, 1d,     */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 030, 1e,     */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 031, 1f,     */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 032, 20, SP  */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__BLANK,
/* 033, 21, !   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 034, 22, "   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 035, 23, #   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 036, 24, $   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 037, 25, %   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 038, 26, &   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 039, 27, '   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 040, 28, (   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 041, 29, )   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 042, 2a, *   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 043, 2b, +   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 044, 2c, ,   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 045, 2d, -   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 046, 2e, .   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 047, 2f, /   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 048, 30, 0   */   __0____|__0____|__DIGIT|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 049, 31, 1   */   __0____|__0____|__DIGIT|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 050, 32, 2   */   __0____|__0____|__DIGIT|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 051, 33, 3   */   __0____|__0____|__DIGIT|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 052, 34, 4   */   __0____|__0____|__DIGIT|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 053, 35, 5   */   __0____|__0____|__DIGIT|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 054, 36, 6   */   __0____|__0____|__DIGIT|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 055, 37, 7   */   __0____|__0____|__DIGIT|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 056, 38, 8   */   __0____|__0____|__DIGIT|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 057, 39, 9   */   __0____|__0____|__DIGIT|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 058, 3a, :   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 059, 3b, ;   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 060, 3c, <   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 061, 3d, =   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 062, 3e, >   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 063, 3f, ?   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 064, 40, @   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 065, 41, A   */   __UPPER|__0____|__0____|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 066, 42, B   */   __UPPER|__0____|__0____|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 067, 43, C   */   __UPPER|__0____|__0____|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 068, 44, D   */   __UPPER|__0____|__0____|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 069, 45, E   */   __UPPER|__0____|__0____|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 070, 46, F   */   __UPPER|__0____|__0____|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 071, 47, G   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 072, 48, H   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 073, 49, I   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 074, 4a, J   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 075, 4b, K   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 076, 4c, L   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 077, 4d, M   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 078, 4e, N   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 079, 4f, O   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 080, 50, P   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 081, 51, Q   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 082, 52, R   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 083, 53, S   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 084, 54, T   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 085, 55, U   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 086, 56, V   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 087, 57, W   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 088, 58, X   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 089, 59, Y   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 090, 5a, Z   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 091, 5b, [   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 092, 5c, \   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 093, 5d, ]   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 094, 5e, ^   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 095, 5f, _   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 096, 60, `   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 097, 61, a   */   __0____|__LOWER|__0____|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 098, 62, b   */   __0____|__LOWER|__0____|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 099, 63, c   */   __0____|__LOWER|__0____|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 100, 64, d   */   __0____|__LOWER|__0____|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 101, 65, e   */   __0____|__LOWER|__0____|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 102, 66, f   */   __0____|__LOWER|__0____|__HEXAD|__PRINT|__0____|__0____|__0____,
/* 103, 67, g   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 104, 68, h   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 105, 69, i   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 106, 6a, j   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 107, 6b, k   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 108, 6c, l   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 109, 6d, m   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 110, 6e, n   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 111, 6f, o   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 112, 70, p   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 113, 71, q   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 114, 72, r   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 115, 73, s   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 116, 74, t   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 117, 75, u   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 118, 76, v   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 119, 77, w   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 120, 78, x   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 121, 79, y   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 122, 7a, z   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 123, 7b, {   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 124, 7c, |   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 125, 7d, }   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 126, 7e, ~   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 127, 7f, DEL */   __0____|__0____|__0____|__0____|__0____|__0____|__CNTRL|__0____,
/* 128, 80, Ç   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 129, 81, ü   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 130, 82, é   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 131, 83, â   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 132, 84, ä   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 133, 85, à   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 134, 86, å   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 135, 87, ç   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 136, 88, ê   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 137, 89, ë   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 138, 8a, è   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 139, 8b, ï   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 140, 8c, î   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 141, 8d, ì   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 142, 8e, Ä   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 143, 8f, Å   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 144, 90, É   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 145, 91, æ   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 146, 92, Æ   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 147, 93, ô   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 148, 94, ö   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 149, 95, ò   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 150, 96, û   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 151, 97, ù   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 152, 98, ÿ   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 153, 99, Ö   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 154, 9a, Ü   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 155, 9b, ø   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 156, 9c, £   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 157, 9d, Ø   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 158, 9e, ×   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 159, 9f, ƒ   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 160, a0, á   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 161, a1, í   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 162, a2, ó   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 163, a3, ú   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 164, a4, ñ   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 165, a5, Ñ   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 166, a6, ª   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 167, a7, º   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 168, a8, ¿   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 169, a9, ®   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 170, aa, ¬   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 171, ab, ½   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 172, ac, ¼   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 173, ad, ¡   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 174, ae, «   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 175, af, »   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 176, b0, ¦   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 177, b1, ¦   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 178, b2, ¦   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 179, b3, ¦   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 180, b4, ¦   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 181, b5, Á   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 182, b6, Â   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 183, b7, À   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 184, b8, ©   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 185, b9, ¦   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 186, ba, ¦   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 187, bb, +   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 188, bc, +   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 189, bd, ¢   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 190, be, ¥   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 191, bf, +   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 192, c0, +   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 193, c1, -   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 194, c2, -   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 195, c3, +   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 196, c4, -   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 197, c5, +   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 198, c6, ã   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 199, c7, Ã   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 200, c8, +   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 201, c9, +   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 202, ca, -   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 203, cb, -   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 204, cc, ¦   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 205, cd, -   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 206, ce, +   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 207, cf, ¤   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 208, d0, ð   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 209, d1, Ð   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 210, d2, Ê   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 211, d3, Ë   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 212, d4, È   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 213, d5, i   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 214, d6, Í   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 215, d7, Î   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 216, d8, Ï   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 217, d9, +   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 218, da, +   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 219, db, ¦   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 220, dc, _   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 221, dd, ¦   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 222, de, Ì   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 223, df, ¯   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 224, e0, Ó   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 225, e1, ß   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 226, e2, Ô   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 227, e3, Ò   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 228, e4, õ   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 229, e5, Õ   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 230, e6, µ   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 231, e7, þ   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 232, e8, Þ   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 233, e9, Ú   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 234, ea, Û   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 235, eb, Ù   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 236, ec, ý   */   __0____|__LOWER|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 237, ed, Ý   */   __UPPER|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 238, ee, ¯   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 239, ef, ´   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 240, f0, ­   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 241, f1, ±   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 242, f2, =   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 243, f3, ¾   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 244, f4, ¶   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 245, f5, §   */   __0____|__0____|__0____|__0____|__PRINT|__PUNCT|__0____|__0____,
/* 246, f6, ÷   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 247, f7, ¸   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 248, f8, °   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 249, f9, ¨   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 250, fa, ·   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 251, fb, ¹   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 252, fc, ³   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 253, fd, ²   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 254, fe, ¦   */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____,
/* 255, ff,     */   __0____|__0____|__0____|__0____|__PRINT|__0____|__0____|__0____};


/* The lusL_Buffer implementation is faster than a solution written in plain C. */
static int _str_reverse (lua_State *L) {
  size_t l;
  const char *s;
  luaL_Buffer b;
  s = luaL_checklstring(L, 1, &l);
  if (lua_gettop(L) == 1) {
    luaL_buffinit(L, &b);
    while (l--) luaL_addchar(&b, s[l]);  /* first decrease l, then call addchar */
    luaL_pushresult(&b);
  } else {  /* UNDOC, 2.25.3 */
    if (s == NULL) {  /* 2.25.4 patch, leave the original string untouched. */
      lua_pushstring(L, "");
    } else {
      char *r;
      r = tools_strndup(s, l);  /* 2.27.0 optimisation */
      tools_strnrev(r, l);
      lua_pushlstring(L, r, l);
      xfree(r);
    }
  }
  return 1;
}


static int str_repeat (lua_State *L) {  /* extended 2.22.0 */
  int n;
  size_t l, lsep;
  const char *s, *sep;
  luaL_Buffer b;
  s = luaL_checklstring(L, 1, &l);
  n = agnL_checkint(L, 2);
  sep = luaL_optlstring(L, 3, "", &lsep);
  if (n <= 0) {
    lua_pushliteral(L, "");
  } else if (l + lsep < l || l + lsep > MAXSIZE/n) {  /* may overflow? */
    return luaL_error(L, "Error in " LUA_QS ": resulting string is too large.", "strings.repeat");
  } else {
    luaL_buffinit(L, &b);
    while (n-- > 1) {  /* first n-1 copies (followed by separator) */
      luaL_addlstring(&b, s, l);
      if (lsep > 0) luaL_addlstring(&b, sep, lsep);
    }
    luaL_addlstring(&b, s, l);  /* last copy (not followed by separator) */
    luaL_pushresult(&b);
  }
  return 1;
}


static int str_tochars (lua_State *L) {
  int i, n, c, nbytes;
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  nbytes = luaL_optint(L, 2, 1);
  if (lua_isseq(L, 1)) {  /* 2.6.1 extension */
    n = agn_seqsize(L, 1);
    if (nbytes == 1) {
      for (i=1; i <= n; i++) {
        c = agn_seqgetinumber(L, 1, i);
        if (uchar(c) != c) {
          luaL_clearbuffer(&b);  /* 2.14.9 improvement */
          luaL_error(L, "Error in " LUA_QS ": invalid value at index %d.", "strings.tochars", i);
        }
        if (c == 0) break;  /* 2.6.1, embedded zero ? */
        luaL_addchar(&b, uchar(c));
      }
    } else {  /* sequence contains unsigned word-aligned uint32_t's; 2.22.2 */
      int j, s, tolittle;
      uint32_t y;
      unsigned char *src;
      tolittle = agnL_optboolean(L, 3, 1);
      (void)tolittle;  /* not used on Little Endian systems */
      s = sizeof(uint32_t);
      for (i=1; i <= n; i++) {
        c = agn_seqgetinumber(L, 1, i);
        if (c == 0) break;  /* 2.6.1, embedded zero ? */
        y = c;
        #if BYTE_ORDER == BIG_ENDIAN
        if (tolittle) tools_swapuint32_t(&y);
        #endif
        src = (unsigned char *)&y;
        for (j=0; j < s; j++) {
          c = uchar(src[j]);
          if (c == 0) break;  /* avoid strings with embedded zeros in result */
          luaL_addchar(&b, c);
        }
      }
    }
  } else {
    n = lua_gettop(L);  /* number of arguments */
    luaL_checkstack(L, n, "too many arguments");  /* 2.6.1 */
    for (i=1; i <= n; i++) {
      c = agnL_checkint(L, i);
      luaL_argcheck(L, uchar(c) == c, i, "invalid value");
      if (c == 0) break;  /* 2.6.1, embedded zero ? */
      luaL_addchar(&b, uchar(c));
    }
  }
  luaL_pushresult(&b);
  return 1;
}


static int str_tobytes (lua_State *L) {
  int tobigendian;
  size_t i, l, uint32;
  const char *str;
  str = luaL_checklstring(L, 1, &l);
  uint32 = (lua_type(L, 2) == LUA_TBOOLEAN) ? lua_toboolean(L, 2) : luaL_optint(L, 2, 0);
  tobigendian = agnL_optboolean(L, 3, 0);
  if (!uint32) {  /* single unsigned bytes, the default */
    agn_createseq(L, l);
    for (i=0; i < l; i++) {
      agn_seqsetinumber(L, -1, i + 1, cast_num(uchar(str[i])));
    }
  } else {  /* 2.17.2 extension: return 4-byte unsigned integers instead of individual ASCII values */
    size_t chunks;
    uint32_t *a = tools_strtouint32s(str, l, &chunks, tobigendian);
    if (!a)
      luaL_error(L, "Error in " LUA_QS ": cannot convert.", "strings.tobytes");
    /* if size l excluding terminal \0 is a multiple of AGN_BLOCKSIZE, drop last entry in array */
    agn_createseq(L, chunks);
    for (i=0; i < chunks; i++) {
      agn_seqsetinumber(L, -1, i + 1, a[i]);
    }
    xfree(a);
  }
  return 1;
}


/* The function converts an integer n in the range 0 .. 0x10ffff = 1114111d into its Unicode representation, a string.
   If n is 0, returns '\0', not ''. If n is out-of-bounds, the function returns `null`.
   Based on David Kolf's Lua JSON package unichar function and strings.tochars; 5.1.5

local procedure unichar(value) is
   if value < 0 then
      return null
   elif value <= 0x007f then
      return _strtochars(value)
   elif value <= 0x07ff then
      return _strtochars(0xc0 + int(value/0x40),
                             0x80 + (int(value) % 0x40))
   elif value <= 0xffff then
      return _strtochars(0xe0 + int(value/0x1000),
                             0x80 + (int(value/0x40) % 0x40),
                             0x80 + (int(value) % 0x40))
   elif value <= 0x10ffff then
      return _strtochars(0xf0 + int(value/0x40000),
                             0x80 + (int(value/0x1000) % 0x40),
                             0x80 + (int(value/0x40) % 0x40),
                             0x80 + (int(value) % 0x40))
   else
      return null
   fi
end; */

static int str_itouni (lua_State *L) {
  luaL_Buffer b;
  lua_Integer n = agn_checkinteger(L, 1);
  if (n < 0 || n > 0x10ffff) {
    lua_pushnil(L);
    return 1;
  }
  luaL_buffinit(L, &b);
  if (n == 0) {
    luaL_addchar(&b, uchar(0));
  } else if (n <= 0x007f) {  /* n <= 127, skip embedded zero */
    luaL_addchar(&b, uchar(n));
  } else if (n <= 0x07ff) {  /* n <= 2047 */
    luaL_addchar(&b, uchar(0xc0 + n/0x40));
    luaL_addchar(&b, uchar(0x80 + (n % 0x40)));
  } else if (n <= 0xffff) {  /* n <= 65535 */
    luaL_addchar(&b, uchar(0xe0 + n/0x1000));
    luaL_addchar(&b, uchar(0x80 + ((n/0x40) % 0x40)));
    luaL_addchar(&b, uchar(0x80 + (n % 0x40)));
  } else {  /* n <= 0x10ffff = 1114111d */
    luaL_addchar(&b, uchar(0xf0 + n/0x40000));
    luaL_addchar(&b, uchar(0x80 + ((n/0x1000) % 0x40)));
    luaL_addchar(&b, uchar(0x80 + ((n/0x40) % 0x40)));
    luaL_addchar(&b, uchar(0x80 + (n % 0x40)));
  }
  luaL_pushresult(&b);
  return 1;
}


static int writer (lua_State *L, const void* b, size_t size, void* B) {  /* reintroduced 1.6.0 */
  (void)L;
  luaL_addlstring((luaL_Buffer*)B, (const char *)b, size);
  return 0;
}

static int str_dump (lua_State *L) {  /* reintroduced 1.6.0, binarily serialise functions */
  luaL_Buffer b;
  int strip = agnL_optboolean(L, 2, 0);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  if (lua_iscfunction(L, 1)) {
    luaL_clearbuffer(&b);  /* 4.0.2 improvement */
    luaL_error(L, "Error in `strings.dump`: cannot dump C library functions.");  /* 1.6.1 */
  }
  lua_settop(L, 1);
  luaL_buffinit(L, &b);
  if (lua_dump(L, writer, &b, strip) != 0) {
    luaL_clearbuffer(&b);  /* 4.0.2 improvement */
    luaL_error(L, "Error in `strings.dump`: unable to dump given function.");  /* 1.6.1 */
  }
  luaL_pushresult(&b);
  return 1;
}


/*
** {======================================================
** PATTERN MATCHING
** =======================================================
*/


#define CAP_UNFINISHED   (-1)
#define CAP_POSITION   (-2)
#define L_ESC      '%'


static int check_capture (MatchState *ms, int l) {
  l -= '1';
  if (l < 0 || l >= ms->level || ms->capture[l].len == CAP_UNFINISHED)
    return luaL_error(ms->L, "invalid capture index.");
  return l;
}


static int capture_to_close (MatchState *ms) {
  int level = ms->level;
  for (level--; level>=0; level--)
    if (ms->capture[level].len == CAP_UNFINISHED) return level;
  return luaL_error(ms->L, "invalid pattern capture.");
}


const char *classend (MatchState *ms, const char *p) {
  switch (*p++) {
    case L_ESC: {
      if (*p == '\0')
        luaL_error(ms->L, "malformed pattern (ends with " LUA_QL("%%") ").");
      return p + 1;
    }
    case '[': {
      if (*p == '^') p++;
      do {  /* look for a `]' */
        if (*p == '\0')
          luaL_error(ms->L, "malformed pattern (missing " LUA_QL("]") ").");
        if (*(p++) == L_ESC && *p != '\0')
          p++;  /* skip escapes (e.g. `%]') */
      } while (*p != ']');
      return p + 1;
    }
    default: {
      return p;
    }
  }
}


static int match_class (int c, int cl) {
  int res;
  switch (tolower(cl)) {
    case 'a' : res = isalpha(c); break;
    case 'c' : res = iscntrl(c); break;
    case 'd' : res = isdigit(c); break;
    case 'k' : res = tools_isconsonant(c); break;
    case 'l' : res = islower(c); break;
    case 'o' : res = tools_isalphadia(c); break;
    case 'p' : res = ispunct(c); break;
    case 's' : res = isspace(c); break;
    case 'u' : res = isupper(c); break;
    case 'v' : res = tools_isvowel(c, 1); break;
    case 'w' : res = isalnum(c); break;
    case 'x' : res = isxdigit(c); break;
    case 'z' : res = (c == 0); break;
    default: return (cl == c);
  }
  return (islower(cl) ? res : !res);
}


int matchbracketclass (int c, const char *p, const char *ec) {
  int sig = 1;
  if (*(p + 1) == '^') {
    sig = 0;
    p++;  /* skip the `^' */
  }
  while (++p < ec) {
    if (*p == L_ESC) {
      p++;
      if (match_class(c, uchar(*p)))
        return sig;
    }
    else if ((*(p + 1) == '-') && (p + 2 < ec)) {
      p += 2;
      if (uchar(*(p - 2)) <= c && c <= uchar(*p))
        return sig;
    }
    else if (uchar(*p) == c) return sig;
  }
  return !sig;
}


int singlematch (int c, const char *p, const char *ep) {
  switch (*p) {
    case '.': return 1;  /* matches any char */
    case L_ESC: return match_class(c, uchar(*(p + 1)));
    case '[': return matchbracketclass(c, p, ep - 1);
    default:  return (uchar(*p) == c);
  }
}


const char *matchbalance (MatchState *ms, const char *s,
                                   const char *p) {
  if (*p == 0 || *(p + 1) == 0)
    luaL_error(ms->L, "unbalanced pattern.");
  if (*s != *p) return NULL;
  else {
    int b = *p;
    int e = *(p + 1);
    int cont = 1;
    while (++s < ms->src_end) {
      if (*s == e) {
        if (--cont == 0) return s + 1;
      }
      else if (*s == b) cont++;
    }
  }
  return NULL;  /* string ends out of balance */
}


const char *max_expand (MatchState *ms, const char *s,
                                 const char *p, const char *ep, int mode) {
  ptrdiff_t i = 0;  /* counts maximum expand for item */
  while ((s + i) < ms->src_end && singlematch(uchar(*(s + i)), p, ep))
    i++;
  /* keeps trying to match with the maximum repetitions */
  while (i >= 0) {
    const char *res = match(ms, (s + i), ep + 1, mode);
    if (res) return res;
    i--;  /* else didn't match; reduce 1 repetition to try again */
  }
  return NULL;
}


const char *min_expand (MatchState *ms, const char *s,
                                 const char *p, const char *ep, int mode) {
  for (;;) {
    const char *res = match(ms, s, ep + 1, mode);
    if (res != NULL)
      return res;
    else if (s < ms->src_end && singlematch(uchar(*s), p, ep))
      s++;  /* try with one more repetition */
    else return NULL;
  }
}


const char *start_capture (MatchState *ms, const char *s,
                                    const char *p, int what, int mode) {
  const char *res;
  int level = ms->level;
  if (level >= LUA_MAXCAPTURES) luaL_error(ms->L, "too many captures.");
  ms->capture[level].init = s;
  ms->capture[level].len = what;
  ms->level = level + 1;
  if ((res=match(ms, s, p, mode)) == NULL)  /* match failed? */
    ms->level--;  /* undo capture */
  return res;
}


const char *end_capture (MatchState *ms, const char *s,
                                  const char *p, int mode) {
  int l = capture_to_close(ms);
  const char *res;
  ms->capture[l].len = s - ms->capture[l].init;  /* close capture */
  if ((res = match(ms, s, p, mode)) == NULL)  /* match failed? */
    ms->capture[l].len = CAP_UNFINISHED;  /* undo capture */
  return res;
}


const char *match_capture (MatchState *ms, const char *s, int l) {
  size_t len;
  l = check_capture(ms, l);
  len = ms->capture[l].len;
  if ((size_t)(ms->src_end-s) >= len &&
      tools_memcmp(ms->capture[l].init, s, len) == 0)
    return s + len;
  else return NULL;
}


/* Reads a number, possible preceded by a sign. The number may include a decimal dot and may also include
   a fractional part. The number may also consist of just an optional sign, followed by a dot and a
   fractional part. Scientific E-notation suffices are supported. 2.32.1 */

#define issign(s) ((uchar(s) == '+' || uchar(s) == '-'))
const char *matchnumber (MatchState *ms, const char *s, const char *p, int *rc) {
  int m, prefix, postfix, scientific;
  unsigned char sep = (uchar(*p) == 'n') ? '.' : ',';
  prefix = postfix = scientific = 0;
  /* skip sign if present */
  if (s < ms->src_end - 1 && issign(*s) && isdigit(uchar(*(s + 1)))) s++;
  else if (s < ms->src_end - 2 && issign(*s) && uchar(*(s + 1)) == sep && isdigit(uchar(*(s + 2)))) s++;
  /* read integral part if present */
  while (s < ms->src_end && isdigit(uchar(*s))) { s++; prefix = 1; }
  /* read fractional part if present */
  if (s < ms->src_end && uchar(*s) == sep) {
    s++;
    while (s < ms->src_end && isdigit(uchar(*s))) { s++; postfix = 1; }
  }
  m = prefix || postfix;
  if (m && s < ms->src_end - 1 && tolower(uchar(*s)) == 'e') {  /* exponential part e or E ? */
    s++;  /* skip e/E */
    if (issign(*s)) s++;  /* skip sign if present */
    while (s < ms->src_end && isdigit(uchar(*s))) { s++; scientific = 1; }
    if (!scientific) m = 0;
  }
  *rc = m;
  s -= m;  /* s will be incremented in switch/default statement of the calling function */
  return s;
}


/* Reads an integer, possible preceded by a sign. 2.32.1 */
const char *matchinteger (MatchState *ms, const char *s, int *rc) {
  int m = 0;
  if (s < ms->src_end - 1 &&  /* skip sign if present */
     issign(*s) && isdigit(uchar(*(s + 1)))) s++;
  while (s < ms->src_end && isdigit(uchar(*s))) { s++; m = 1; }  /* read integral part */
  *rc = m;
  s -= m;  /* s will be incremented in switch/default statement of the calling function */
  return s;
}

/* mode = 1: call from Lua API, mode = 0: call from VM (for properly issuing errors from either API or VM) */
const char *match (MatchState *ms, const char *s, const char *p, int mode) {
  init: /* using goto's to optimize tail recursion */
  switch (*p) {
    case '(': {  /* start capture */
      if (*(p + 1) == ')')  /* position capture? */
        return start_capture(ms, s, p + 2, CAP_POSITION, mode);
      else
        return start_capture(ms, s, p + 1, CAP_UNFINISHED, mode);
    }
    case ')': {  /* end capture */
      return end_capture(ms, s, p + 1, mode);
    }
    case L_ESC: {
      switch (*(p + 1)) {
        case 'b': {  /* balanced string? */
          s = matchbalance(ms, s, p + 2);
          if (s == NULL) return NULL;
          p += 4; goto init;
        }
        case 'f': {  /* frontier? */
          const char *ep; char previous;
          p += 2;
          if (*p != '[') {
            if (mode)
              luaL_error(ms->L, "missing " LUA_QL("[") " after "
                               LUA_QL("%%f") " in pattern.");
            else
              luaG_runerror(ms->L, "missing " LUA_QL("[") " after "
                               LUA_QL("%%f") " in pattern.");
          }
          ep = classend(ms, p);  /* points to what is next */
          previous = (s == ms->src_init) ? '\0' : *(s - 1);
          if (matchbracketclass(uchar(previous), p, ep - 1) ||
             !matchbracketclass(uchar(*s), p, ep - 1)) return NULL;
          p = ep; goto init;
        }
        default: {
          if (isdigit(uchar(*(p + 1)))) {  /* capture results (%0-%9)? */
            s = match_capture(ms, s, uchar(*(p + 1)));
            if (s == NULL) return NULL;
            p += 2; goto init;
          }
          goto dflt;  /* case default */
        }
      }
    }
    case '\0': {  /* end of pattern */
      return s;  /* match succeeded */
    }
    case '$': {
      if (*(p + 1) == '\0')  /* is the `$' the last char in pattern? */
        return (s == ms->src_end) ? s : NULL;  /* check end of string */
      else goto dflt;
    }
    default: dflt: {  /* it is a pattern item */
      int m;
      const char *ep = classend(ms, p);  /* points to what is next */
      if (*p == L_ESC && tolower(*(p + 1)) == 'n')  /* 2.32.1 */
        s = matchnumber(ms, s, p + 1, &m);
      else if (*p == L_ESC && *(p + 1) == 'i')  /* 2.32.1 */
        s = matchinteger(ms, s, &m);
      else
        m = s < ms->src_end && singlematch(uchar(*s), p, ep);
      switch (*ep) {
        case '?': {  /* optional */
          const char *res;
          if (m && ((res = match(ms, s + 1, ep + 1, mode)) != NULL))
            return res;
          p = ep + 1; goto init;
        }
        case '*': {  /* 0 or more repetitions */
          return max_expand(ms, s, p, ep, mode);
        }
        case '+': {  /* 1 or more repetitions */
          return (m ? max_expand(ms, s + 1, p, ep, mode) : NULL);
        }
        case '-': {  /* 0 or more repetitions (minimum) */
          return min_expand(ms, s, p, ep, mode);
        }
        default: {
          if (!m) return NULL;
          s++; p = ep; goto init;
        }
      }
    }
  }
}


void initMatchState (lua_State *L, MatchState *ms, int level, const char *s, size_t ls) {
  ms->L = L;
  ms->src_init = s;
  ms->src_end = s + ls;
  ms->level = level;  /* 7.9.2 fix */
}

static void push_onecapture (MatchState *ms, int i, const char *s,
                                                    const char *e) {
  if (i >= ms->level) {
    if (i == 0)  /* ms->level == 0, too */
      lua_pushlstring(ms->L, s, e - s);  /* add whole match */
    else
      luaL_error(ms->L, "invalid capture index %%%d", i + 1);  /* adapted to Lua 5.3.5 */
  }
  else {
    ptrdiff_t l = ms->capture[i].len;
    if (l == CAP_UNFINISHED) luaL_error(ms->L, "unfinished capture.");
    if (l == CAP_POSITION)
      lua_pushinteger(ms->L, ms->capture[i].init - ms->src_init + 1);
    else
      lua_pushlstring(ms->L, ms->capture[i].init, l);
  }
}

static int push_captures (MatchState *ms, const char *s, const char *e) {
  int i;
  int nlevels = (ms->level == 0 && s) ? 1 : ms->level;
  luaL_checkstack(ms->L, nlevels, "too many captures");
  for (i=0; i < nlevels; i++)
    push_onecapture(ms, i, s, e);
  return nlevels;  /* number of strings pushed */
}

static int str_find_aux (lua_State *L, int find, int multiple) {
  size_t l1, l2, c;
  const char *s = agn_checklstring(L, 1, &l1);
  const char *p = agn_checklstring(L, 2, &l2);
  ptrdiff_t init = tools_posrelat(agnL_optinteger(L, 3, 1), l1) - 1;
  if (find && l2 == 0) {  /* 2.38.0 */
    lua_pushnil(L);
    return 1;
  }
  if (init < 0) init = 0;
  else if ((size_t)(init) > l1) init = (ptrdiff_t)l1;
  c = 0;
  if (find && (lua_toboolean(L, 4) ||    /* explicit request? */
      !tools_hasstrchr(p, SPECIALS, l2))) {  /* or no special characters?  2.16.5/2.16.12/2.26.3 optimisation */
    /* do a plain search */
    const char *s2 = tools_lmemfind(s + init, l1 - init, p, l2);  /* 2.16.12 tweak */
    if (s2) {
      lua_pushinteger(L, s2 - s + 1);
      lua_pushinteger(L, s2 - s + l2);
      return 2;
    }
  } else {
    MatchState ms;
    int anchor = (*p == '^') ? (p++, 1) : 0;
    char *s1 = (char *)s + init;
    initMatchState(L, &ms, 0, s, l1);
    do {
      const char *res;
      ms.level = 0;
      if ((res = match(&ms, (const char *)s1, p, 1)) != NULL) {
        if (!multiple) {  /* 2.28.5 */
          if (find) {
            lua_pushinteger(L, s1 - s + 1);  /* start */
            lua_pushinteger(L, res - s);     /* end */
            return push_captures(&ms, NULL, 0) + 2;
          } else
            return push_captures(&ms, s1, res);
        } else {
          c++;
          luaL_checkstack(L, 1, "too many captures");
          push_captures(&ms, s1, res);
          s1 = (char *)res;
        }
      }
    } while (s1++ < ms.src_end && !anchor);
    if (c > 0) return c;  /* 2.28.5 */
  }
  lua_pushnil(L);  /* not found */
  return 1;
}

static int aux_processpattern (lua_State *L, int nargs) {
  int i;
  const char *pattern;
  if (!lua_isstring(L, -1)) {
    agn_poptoptwo(L);
    luaL_error(L, "Error in " LUA_QS ": structure must contain strings only.", "strings.find");
  }
  pattern = lua_tostring(L, -1);
  luaL_checkstack(L, nargs, "too many arguments");
  if (agnL_gettablefield(L, "strings", "find", "strings.find", 1) != LUA_TFUNCTION)
    luaL_error(L, "Error in " LUA_QS ": could not find myself.", "strings.find");
  lua_pushvalue(L, 1);
  lua_pushstring(L, pattern);
  for (i=3; i <= nargs; i++) lua_pushvalue(L, i);
  lua_call(L, nargs, 2);
  if (!lua_isnil(L, -1)) {  /* there is a hit */
    lua_remove(L, -3);  /* remove set value */
    lua_remove(L, -3);  /* remove set value */
    lua_pushstring(L, pattern);
    return 3;
  } else {
    lua_pop(L, 3);  /* pop null, null and set value, then proceed with next member */
  }
  return 0;
}

static int str_multfind (lua_State *L, int (*f)(lua_State *, int), int nargs) {  /* 2.32.4 */
  int rc = 0;
  lua_pushnil(L);
  while ((*f)(L, 2)) {
    rc = aux_processpattern(L, nargs);
    if (rc) return rc;
  }
  lua_pushnil(L);
  return 1;
}

static int str_find (lua_State *L) {
  if (lua_isstring(L, 2))
    return str_find_aux(L, 1, 0);
  switch (lua_type(L, 2)) {  /* 2.32.4 extension */
    case LUA_TSET:
      return str_multfind(L, lua_usnext, lua_gettop(L));
    case LUA_TTABLE:
      return str_multfind(L, lua_next, lua_gettop(L));
    case LUA_TSEQ:
      return str_multfind(L, lua_seqnext, lua_gettop(L));
    case LUA_TREG:
      return str_multfind(L, lua_regnext, lua_gettop(L));
    default: {
      luaL_error(L, "Error in " LUA_QS ": expected a string or a structure of strings.", "strings.find");
    }
  }
  return 0;  /* cannot happen */
}


static int str_match (lua_State *L) {
  return str_find_aux(L, 0, 0);
}


static int str_matches (lua_State *L) {
  return str_find_aux(L, 0, 1);
}


static int gmatch_aux (lua_State *L) {
  MatchState ms;
  size_t ls;
  const char *s, *p, *src;
  s = lua_tolstring(L, lua_upvalueindex(1), &ls);
  p = lua_tostring(L, lua_upvalueindex(2));
  initMatchState(L, &ms, 0, s, ls);
  for (src=s + (size_t)lua_tointeger(L, lua_upvalueindex(3));
       src <= ms.src_end;
       src++) {
    const char *e;
    ms.level = 0;
    if ((e = match(&ms, src, p, 1)) != NULL) {
      lua_Integer newstart = e - s;
      if (e == src) newstart++;  /* empty match? go at least one position */
      lua_pushinteger(L, newstart);
      lua_replace(L, lua_upvalueindex(3));
      return push_captures(&ms, src, e);
    }
  }
  return 0;  /* not found */
}

static int gmatch (lua_State *L) {
  luaL_checkstring(L, 1);
  luaL_checkstring(L, 2);
  lua_settop(L, 2);
  lua_pushinteger(L, 0);
  lua_pushcclosure(L, gmatch_aux, 3);
  return 1;
}


static void add_s (MatchState *ms, luaL_Buffer *b, const char *s,
                                                   const char *e) {
  size_t l, i;
  const char *news = lua_tolstring(ms->L, 3, &l);
  for (i=0; i < l; i++) {
    if (news[i] != L_ESC)
      luaL_addchar(b, news[i]);
    else {
      i++;  /* skip ESC */
      if (!isdigit(uchar(news[i])))
        luaL_addchar(b, news[i]);
      else if (news[i] == '0')
          luaL_addlstring(b, s, e - s);
      else {
        push_onecapture(ms, news[i] - '1', s, e);
        luaL_addvalue(b);  /* add capture to accumulated result */
      }
    }
  }
}


static void add_value (MatchState *ms, luaL_Buffer *b, const char *s,
                                                       const char *e) {
  lua_State *L = ms->L;
  switch (lua_type(L, 3)) {
    case LUA_TNUMBER:
    case LUA_TSTRING: {
      add_s(ms, b, s, e);
      return;
    }
    case LUA_TFUNCTION: {
      int n;
      lua_pushvalue(L, 3);
      n = push_captures(ms, s, e);
      lua_call(L, n, 1);
      break;
    }
    case LUA_TTABLE: {
      push_onecapture(ms, 0, s, e);
      lua_gettable(L, 3);
      break;
    }
  }
  if (!lua_toboolean(L, -1)) {  /* nil or false? */
    agn_poptop(L);
    lua_pushlstring(L, s, e - s);  /* keep original text */
  }
  else if (!agn_isstring(L, -1))
    luaL_error(L, "invalid replacement value (a %s).", luaL_typename(L, -1));
  luaL_addvalue(b);  /* add result to accumulator */
}


static int str_gsub (lua_State *L) {
  size_t srcl;
  const char *src = luaL_checklstring(L, 1, &srcl);
  const char *p = luaL_checkstring(L, 2);
  int tr = lua_type(L, 3);  /* 5.1.3 patch */
  int max_s = agnL_optinteger(L, 4, srcl + 1);
  int anchor = (*p == '^') ? (p++, 1) : 0;
  int n = 0;
  MatchState ms;
  luaL_Buffer b;
  luaL_argcheck(L, tr == LUA_TNUMBER || tr == LUA_TSTRING ||  /* 5.1.2 patch */
                   tr == LUA_TFUNCTION || tr == LUA_TTABLE, 3,
                   "string, procedure or table expected");
  luaL_buffinit(L, &b);
  initMatchState(L, &ms, 0, src, srcl);
  while (n < max_s) {
    const char *e;
    ms.level = 0;
    e = match(&ms, src, p, 1);
    if (e) {
      n++;
      add_value(&ms, &b, src, e);
    }
    if (e && e>src) /* non empty match? */
      src = e;  /* skip it */
    else if (src < ms.src_end)
      luaL_addchar(&b, *src++);
    else break;
    if (anchor) break;
  }
  luaL_addlstring(&b, src, ms.src_end - src);
  luaL_pushresult(&b);
  lua_pushinteger(L, n);  /* number of substitutions */
  return 2;
}


/* Checks whether the given string `pattern' represents a valid pattern/capture accepted by `strings.match`,
  `strings.find`, etc. and returns `true` or `false`. The second return, a Boolean, indicates whether
   `pattern' contains special characters used in patterns or captures, that is anything in  "^$*+?.([%-". */
static int str_ispattern (lua_State *L) {  /* 6.1.1 */
  size_t l;
  int hasspecials, nargs;
  nargs = lua_gettop(L);
  const char *p = luaL_checklstring(L, 1, &l);
  luaL_checkstack(L, 1 + nargs == 1, "not enough stack space");
  if (nargs == 1) {
    lua_pushboolean(L, agnL_isvalidpattern(L, p, l, &hasspecials, 0));
    lua_pushboolean(L, hasspecials);
    return 2;
  } else {
    agnL_isvalidpattern(L, p, l, &hasspecials, 0);
    lua_pushboolean(L, hasspecials);
  }
  return 1 + (nargs == 1);
}


/* }====================================================== */


/* maximum size of each formatted item (> len(format('%99.99f', -1e308))) */
#define MAX_ITEM   512
/* valid flags in a format specification */
#define FLAGS   "-+ #0"
/*
** maximum size of each format specification (such as '%-099.99d')
** (+10 accounts for %99.99x plus margin of error)
*/
#define MAX_FORMAT   (sizeof(FLAGS) + sizeof(LUA_INTFRMLEN) + 13)  /* changed from 10 to 13 to accompany '0b' prefix and 'lf', 2.21.6/2.33.4 */

static void changemodito (char *form, char chr);
static void addintlen (char *form);

static int haslengthspecifier (char *p) {  /* 2.21.6 */
  if (*p != '\0' && (strchr("%", *p) != NULL)) p++; else return 0;  /* skip `%' */
  while (*p != '\0' && (strchr(FLAGS, *p) != NULL)) p++;  /* skip flags */
  return (isdigit(uchar(*p)));
}

/* 0.9.2, 2.4.5, 2.10.0 */
static void addquote (luaL_Buffer *b, int lua) {
  if (lua == 1) luaL_addchar(b, '"');
  else if (lua == 2) luaL_addchar(b, '\'');
  else if (lua == 3) luaL_addchar(b, '`');
  /* else do not add anything */
}

static void addquoted (lua_State *L, luaL_Buffer *b, char *buff, char *form, int arg, int lua) {
  if (haslengthspecifier(form) && agn_isnumber(L, arg)) {  /* size over speed, added 2.21.6 */
    int isint = agn_isinteger(L, arg);
    changemodito(form, (isint) ? 'd' : 'f');
    if (isint) addintlen(form);
    addquote(b, lua);
    if (isint)  /* do NOT contract this into one statement with (isint) ? ... */
      sprintf(buff, form, (LUA_INTFRM_T)agn_tonumber(L, arg));
    else
      sprintf(buff, form, agn_tonumber(L, arg));
    luaL_addstring(b, buff);
    addquote(b, lua);
  } else {
    size_t l;
    const char *s;
    s = luaL_checklstring(L, arg, &l);
    addquote(b, lua);
    while (l--) {
      switch (*s) {
        case '"': case '\\': case '\n': {
          luaL_addchar(b, '\\');
          luaL_addchar(b, *s);
          break;
        }
        case '\r': {
          luaL_addlstring(b, "\\r", 2);
          break;
        }
        case '\0': {
          luaL_addlstring(b, "\\000", 4);
          break;
        }
        default: {
          luaL_addchar(b, *s);
          break;
        }
      }
      s++;
    }
    addquote(b, lua);
  }
}

static const char *scanformat (lua_State *L, const char *strfrmt, char *form) {
  /* returns format token w/o `%` indicator or sizes */
  const char *p = strfrmt;
  while (*p != '\0' && strchr(FLAGS, *p) != NULL) p++;  /* skip flags - Lua 5.1.2 patch */
  if ((size_t)(p - strfrmt) >= sizeof(FLAGS))
    luaL_error(L, "invalid format (repeated flags).");
  if (isdigit(uchar(*p))) p++;  /* skip width */
  if (isdigit(uchar(*p))) p++;  /* (2 digits at most) */
  if (*p == '.') {
    p++;
    if (isdigit(uchar(*p))) p++;  /* skip precision */
    if (isdigit(uchar(*p))) p++;  /* (2 digits at most) */
  }
  if (isdigit(uchar(*p)))
    luaL_error(L, "invalid format (width or precision too long)");
  *(form++) = '%';
  strncpy(form, strfrmt, p - strfrmt + 1);
  form += p - strfrmt + 1;
  *form = '\0';
  return p;
}

static void addintlen (char *form) {
  size_t l;
  char spec;
  l = tools_strlen(form);  /* 2.17.8 tweak */
  spec = form[l - 1];
  strcpy(form + l - 1, LUA_INTFRMLEN);
  form[l + sizeof(LUA_INTFRMLEN) - 2] = spec;
  form[l + sizeof(LUA_INTFRMLEN) - 1] = '\0';
}

static void addmonlen (char *form) {  /* 2.10.0 */
  size_t l;
  l = tools_strlen(form);  /* 2.17.8 tweak */
  strcpy(form + l - 1, ".2f");
  form[l + 2] = '\0';
}

static void addpercent (char *form) {  /* 2.10.0 */
  size_t l;
  l = tools_strlen(form);  /* 2.17.8 tweak */
  strcpy(form + l - 1, "f%%");
  form[l + 2] = '\0';
}

static void changespec (char *form, int isnumber) {  /* 2.4.5 */
  size_t l = tools_strlen(form);  /* 2.17.8 tweak */
  char spec = form[l - 1];
  if (!isnumber) spec = 'F';
  form[l - 1] = tolower(spec);
}

static void changemodito (char *form, char chr) {  /* 2.10.0 */
  form[tools_strlen(form) - 1] = chr;  /* 2.17.8 tweak */
}

/* See: GNU C Library/Setting-the-Locale.html#index-setlocale, 2.10.0 */
static void numcurlocale (lua_State *L, char *buff, char *form, int arg, char from, char to) {
  char *old_locale, *saved_locale;
  /* get the name of the current locale */
  old_locale = setlocale(LC_ALL, NULL);
  /* copy the name so it won’t be clobbered by setlocale */
  if (!old_locale)  /* 2.17.1 fix */
    luaL_error(L, "Error in " LUA_QS ": unexpected error.", "strings.format");
  saved_locale = tools_strdup(old_locale);
  if (saved_locale == NULL)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "strings.format");
  /* now change the locale and do some stuff with it, see:
     http://stackoverflow.com/questions/12170488/how-to-get-current-locale-of-my-environment */
  changemodito(form, to);
  setlocale(LC_ALL, "");
  if (from == 'n')
    sprintf(buff, form, (double)agn_checknumber(L, arg));
  else {
    int isnumber = lua_isnumber(L, arg);
    changespec(form, isnumber);
    if (isnumber)
      sprintf(buff, form, (double)agn_tonumber(L, arg));
    else
      sprintf(buff, form, (double)AGN_NAN);
  }
  /* restore the original locale */
  setlocale(LC_ALL, saved_locale);
  xfree(saved_locale);
}

/* based on the StackOverflow article `Digit grouping in C's printf', modified. 2.10.0 */
static void monamount (lua_State *L, char *buff, char *form, int arg) {
  size_t tail, isnonnumber, c, i;
  double x;
  char *result_p, *decpoint, *thousep, *ch;
  static char result[MAX_ITEM];
  struct lconv *lc;
  x = (double)agn_checknumber(L, arg);
  result_p = result;
  lc = localeconv();
  decpoint = lc->mon_decimal_point;
  thousep = lc->mon_thousands_sep;
  if (tools_strlen(decpoint) == 0) decpoint = ".";  /* 2.17.8 tweak */
  if (tools_strlen(thousep) == 0) thousep = ",";  /* 2.17.8 tweak */
  if (strchr(form, '.') == 0)  /* no decimal precision given */
    addmonlen(form);
  else
    changemodito(form, 'f');
  /* snprintf(result, sizeof(result), form, x); */
  sprintf(result, form, x);
  while (*result_p != '\0' && *result_p != *decpoint) result_p++;
  tail = result + sizeof(result) - result_p;
  c = 0;
  while (result_p - result > 3) {
    result_p -= 3;
    ch = result_p - 1;
    isnonnumber = isspace(*ch) || *ch == '-' || *ch == '+';  /* modified */
    if (!isnonnumber) {
      memmove(result_p + 1, result_p, tail);  /* extends the result string */
      *result_p = *thousep;
      c++;
    }
    tail += 4 - isnonnumber;
  }
  result_p = result;
  for (i=0; i < c && isspace(*result_p); i++) result_p++;  /* remove c leading spaces if possible, modified */
  strcpy(buff, result_p);
}

static unsigned char convertbase_hash[36] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
  'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'};

static long long int convertbase (lua_State *L, long int x, long int source, long int target) {
  luaL_Buffer b;
  long int v;
  long long int r;
  char *tail;
  const char *s;
  size_t c;
  int digit;
  if (source == 10 && target == 10) return x;  /* 3.10.3 fix */
  luaL_buffinit(L, &b);
  v = 0;
  if (x < 0) x = -x;
  for (c=0; x > 0; c++) {
    digit = x % 10L;
    v += digit * tools_intpow(source, c);
    x /= 10L;
  }
  x = (long int)v;
  c = 0;
  while (x > 0) {
    c++;
    luaL_addchar(&b, convertbase_hash[x % target]);
    x /= target;
  }
  luaL_pushresult(&b);
  s = lua_tostring(L, -1);
  luaL_clearbuffer(&b);
  luaL_buffinit(L, &b);
  /* reverse string */
  while (c--) luaL_addchar(&b, s[c]);
  luaL_pushresult(&b);
  errno = 0;
  r = strtoll(lua_tostring(L, -1), &tail, 10);
  agn_poptoptwo(L);
  if (r >= LONG_MAX) {
    luaL_clearbuffer(&b);  /* 4.0.2 improvement */
    luaL_error(L, "Error in " LUA_QS ": value is out of range.", "strings.format");
  }
  return r;
}

#define L_FMTFLAGSC   "-"

static const char *get2digits (const char *s) {
  if (isdigit(uchar(*s))) {
    s++;
    if (isdigit(uchar(*s))) s++;  /* (2 digits at most) */
  }
  return s;
}

#define DLong ieee_long_double_shape_type
#define checkandgetdlong(L,idx) (((DLong *)luaL_checkudata(L, idx, "longdouble"))->value)

/*
** Check whether a conversion specification is valid. When called,
** first character in 'form' must be '%' and last character must
** be a valid conversion specifier. 'flags' are the accepted flags;
** 'precision' signals whether to accept a precision. Taken from Lua 5.4.4, 2.27.10
*/
static void checkformat (lua_State *L, const char *form, const char *flags,
                                       int precision) {
  const char *spec = form + 1;  /* skip '%' */
  spec += strspn(spec, flags);  /* skip flags */
  if (*spec != '0') {  /* a width cannot start with '0' */
    spec = get2digits(spec);  /* skip width */
    if (*spec == '.' && precision) {
      spec++;
      spec = get2digits(spec);  /* skip precision */
    }
  }
  if (!isalpha(uchar(*spec)))  /* did not go to the end? */
    luaL_error(L, "invalid conversion specification: '%s'", form);
}

static int str_format (lua_State *L) {
  int top, arg;
  size_t sfl;
  const char *strfrmt, *strfrmt_end;
  luaL_Buffer b;
#if defined(_WIN32)
  struct WinVer winversion;
#endif
  top = lua_gettop(L);
  arg = 1;
  strfrmt = luaL_checklstring(L, arg, &sfl);
  strfrmt_end = strfrmt + sfl;
  luaL_buffinit(L, &b);
  while (strfrmt < strfrmt_end) {
    if (*strfrmt != L_ESC)
      luaL_addchar(&b, *strfrmt++);
    else if (*++strfrmt == L_ESC)
      luaL_addchar(&b, *strfrmt++);  /* %% */
    else { /* format item */
      char form[MAX_FORMAT];   /* to store the format (`%...') */
      char buff[MAX_ITEM];     /* to store the formatted item */
      int maxitem = MAX_ITEM;  /* maximum length for the result, 2.27.10 */
      if (++arg > top)  /* Lua 5.1.4 patch 7 */
        luaL_argerror(L, arg, "no value");
      strfrmt = scanformat(L, strfrmt, form);
      switch (*strfrmt++) {
        case 'c': {
          sprintf(buff, form, (int)agn_checknumber(L, arg));
          break;
        }
        case 'd':  case 'i': {
          addintlen(form);
          sprintf(buff, form, (LUA_INTFRM_T)agn_checknumber(L, arg));
          break;
        }
        case 'o': case 'u': case 'x': case 'X': {
          addintlen(form);
          sprintf(buff, form, (unsigned LUA_INTFRM_T)agn_checknumber(L, arg));
          break;
        }
        case 'e': case 'E': case 'f': case 'g': case 'G': {
          sprintf(buff, form, (double)agn_checknumber(L, arg));
          break;
        }
        case 'l': {  /* 2.33.4, note that in C99 'f' = 'lf' */
          size_t l = tools_strlen(form);
          if (*strfrmt == 'f') {
            form[l - 1] = 'f';
            strfrmt++;
            if (tools_streq(form, "%f")) {
              strcpy(form, "%.16f\0");
            } /* maximum length */
            sprintf(buff, form, (double)agn_checknumber(L, arg));
          } else if (*strfrmt == 'e') {  /* 2.41.2 */
            form[l - 1] = 'e';
            strfrmt++;
            if (tools_streq(form, "%e")) {
              strcpy(form, "%.16e\0");
            } /* maximum length */
            sprintf(buff, form, (double)agn_checknumber(L, arg));
          } else if (*strfrmt == 'd') {
            form[l - 1] = 'L';
            form[l - 0] = 'F';
            form[l + 1] = '\0';
            if (tools_streq(form, "%LF")) strcpy(form, "%.19LF\0");  /* maximum length */
            strfrmt++;
            /* long doubles, we have to call __mingw_sprintf in MinGW, as standard sprintf is buggy there */
#ifdef __MINGW32__
            __mingw_sprintf(buff, form, checkandgetdlong(L, arg));
#else
            sprintf(buff, form, checkandgetdlong(L, arg));
#endif
          } else {
            return luaL_error(L,
              "invalid option " LUA_QL("%%%c%c") " to " LUA_QL("format") ".", *(strfrmt - 1), *strfrmt);
          }
          break;
        }
        case 'n': {  /* 2.10.0 */
          numcurlocale(L, buff, form, arg, 'n', 'f');
          break;
        }
        case 'N': {  /* 2.10.0 */
          numcurlocale(L, buff, form, arg, 'N', 'f');
          break;
        }
        case 'm': {  /* 2.10.0 */
          monamount(L, buff, form, arg);
          break;
        }
#if (!(defined(__DJGPP__) || defined(__OS2__)))
        case 'h': {  /* 2.10.0 */
          /* Print a floating-point number in a hexadecimal fractional notation with the exponent to base 2 represented in decimal digits. */
#ifdef _WIN32
          if (getWindowsVersion(&winversion) > MS_WINXP) {
#endif
            changemodito(form, 'a');
            sprintf(buff, form, (double)agn_checknumber(L, arg));
#ifdef _WIN32
          } else {
            return luaL_error(L, /* 2.18.1/2 fix */
              "invalid option " LUA_QL("%%%c") " to " LUA_QL("format") ".", *(strfrmt - 1));
          }
#endif
          break;
        }
        case 'H': {  /* 2.10.0 */
          /* Print a floating-point number in a hexadecimal fractional notation which the exponent to base 2 represented in decimal digits. */
          changemodito(form, 'A');
          sprintf(buff, form, (double)agn_checknumber(L, arg));
          break;
        }
#endif
        case 'D': {  /* 2.4.5 */
          int isnumber = lua_isnumber(L, arg);
          changespec(form, isnumber);
          addintlen(form);
          if (isnumber)
            sprintf(buff, form, (LUA_INTFRM_T)agn_tonumber(L, arg));
          else
            sprintf(buff, form, (double)AGN_NAN);
          break;
        }
        case 'F': {  /* 2.4.5 */
          int isnumber = lua_isnumber(L, arg);
          changespec(form, isnumber);
          if (isnumber)
            sprintf(buff, form, (double)agn_tonumber(L, arg));
          else
            sprintf(buff, form, (double)AGN_NAN);
          break;
        }
        case 'p': {
          addpercent(form);
          sprintf(buff, form, (double)agn_checknumber(L, arg)*100);
          break;
        }
        case 'P': {
          const void *p = lua_topointer(L, arg);
          checkformat(L, form, L_FMTFLAGSC, 0);
          changemodito(form, 'p');
          if (p == NULL) {  /* avoid calling 'printf' with argument NULL */
            p = "(null)";  /* result */
            form[strlen(form) - 1] = 's';  /* format it as a string */
          }
          l_sprintf(buff, maxitem, form, p);
          break;
        }
        case 'a': {
          addquoted(L, &b, buff, form, arg, 0);
          continue;  /* skip `addsize' at the end */
        }
        case 'A': {  /* 2.14.2 */
          addquoted(L, &b, buff, form, arg, 0);
          continue;  /* skip `addsize' at the end */
        }
        case 'q': {
          addquoted(L, &b, buff, form, arg, 1);
          continue;  /* skip `addsize' at the end */
        }
        case 'Q': {  /* 2.10.0 */
          addquoted(L, &b, buff, form, arg, 2);
          continue;  /* skip `addsize' at the end */
        }
        case 'B': {  /* 2.10.0 */
          addquoted(L, &b, buff, form, arg, 3);
          continue;  /* skip `addsize' at the end */
        }
        case 'b':  {  /* 2.12.6 */
          long int x0 = agn_checkinteger(L, arg);
          char *formstr;
          lua_Number x = convertbase(L, x0, 10, 2);
          changemodito(form, 'd');
          addintlen(form);
          formstr = str_concat((x0 < 0) ? "-0b" : "0b", form, NULL);
          if (!formstr) {
            luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "strings.format");
          }
          sprintf(buff, formstr, (LUA_INTFRM_T)x);
          xfree(formstr);
          break;
        }
        case 's': {
          size_t l;
          const char *s;
          /* Author: Doug Currie's strings.format patch, taken from:
             https://github.com/dubiousjim/luafiveq/blob/master/patches/string-format.patch
             extended for Agena 4.7.4 */
          if (!lua_isstring(L, arg)) {
            lua_getglobal(L, "tostringx");  /* defined in lib/library.agn */
            if (!lua_isnil(L, -1)) {  /* function exists ? */
              lua_pushvalue(L, arg);
              lua_call(L, 1, 1);
              lua_replace(L, arg);
            } else {  /* fallback */
              agn_poptop(L);  /* drop null */
              lua_getglobal(L, "tostring");  /* try it with built-in `tostring` */
              if (!lua_isnil(L, -1)) {
                lua_pushvalue(L, arg);
                lua_call(L, 1, 1);
                lua_replace(L, arg);
              } else {
                agn_poptop(L);
              }
            }
          }
          s = luaL_checklstring(L, arg, &l);
          if (!strchr(form, '.') && l >= 100) {
            /* no precision and string is too long to be formatted;
               keep original string */
            lua_pushvalue(L, arg);
            luaL_addvalue(&b);
            continue;  /* skip `addsize' at the end */
          }
          else {
            sprintf(buff, form, s);
            break;
          }
        }
        default: {  /* also treat cases `pnLlh' */
          return luaL_error(L, "Error: invalid option " LUA_QL("%%%c") " to "
                               LUA_QL("strings.format") ".", *(strfrmt - 1));
        }
      }
      luaL_addlstring(&b, buff, tools_strlen(buff));  /* 2.17.8 tweak */
    }
  }
  luaL_pushresult(&b);
  return 1;
}


/*************************************************************************/
/* functions added 0.5.3 and later                                       */
/*************************************************************************/

/* 0.20.0, April 10, 2009; extended January 23, 2010, 0.30.4; modified September 10, 2011, 1.5.0;
   tuned and Valgrind-fixed 1.6.0; patched 2.10.0 */
static int str_isstarting (lua_State *L) {
  size_t p_len, s_len;
  int pos;
  MatchState ms;
  const char *s, *p;
  s = agn_checklstring(L, 1, &s_len);
  p = agn_checklstring(L, 2, &p_len);
  if (p_len == 0 || s_len == 0) {
    /* bail out if string or pattern is empty or if pattern is longer or equal in size */
    lua_pushfalse(L);
    return 1;
  }
  if (lua_toboolean(L, 3) ||  /* explicit request? */
      !tools_hasstrchr(p, SPECIALS, p_len)) {  /* or no special characters? Agena 1.3.2, 2.16.5/2.16.12/2.26.3 optimisation */
    pos = tools_strncmp(s, p, p_len);
    lua_pushboolean(L, pos == 0 && p_len < s_len);
    return 1;
  }
  initMatchState(L, &ms, 0, s, s_len);
  lua_pushboolean(L, match(&ms, s, p, 1) != NULL);
  return 1;
}


/* modified September 10, 2011, 1.5.0; tuned and Valgrind-fixed 1.6.0 */

static int str_isending (lua_State *L) {
  size_t p_len, s_len, flag;
  const char *s, *p;
  s = agn_checklstring(L, 1, &s_len);
  p = agn_checklstring(L, 2, &p_len);
  if (p_len == 0 || s_len == 0) {
    /* bail out if string or pattern is empty or if pattern is longer or equal in size */
    lua_pushfalse(L);
    return 1;
  }
  if (lua_toboolean(L, 3) ||  /* explicit request? */
      !tools_hasstrchr(p, SPECIALS, p_len)) {  /* or no special characters?  Agena 1.3.2, 2.16.5/2.16.12, 2.26.3 optimisation */
    s += s_len - p_len;
    lua_pushboolean(L, strstr(s, p) == s && p_len < s_len);  /* Agena 1.6.0 */
  } else {  /* 6.1.1 patch to avoid memory leaks in case of malformed patterns */
    MatchState ms;
    int i;
    char p1[p_len + 2];  /* we use a variable-length array (VLA) for match() may throw errors later; 6.1.1 patch */
    p1[p_len] = 0; p1[p_len + 1] = 0;
    for (i=0; i < p_len; i++) p1[i] = p[i];
    if (p[p_len - 1] != '$') p1[p_len] = '$';
    initMatchState(L, &ms, 0, s, s_len);
    flag = 0;
    do {  /* inspect each char */
      ms.level = 0;
      flag = (match(&ms, s, p1, 1) != NULL);  /* 2.10.0 patch */
    } while (s++ < ms.src_end && !flag);  /* 2.10.0 patch */
    lua_pushboolean(L, flag);
  }
  return 1;
}


static int str_ismagic (lua_State *L) {  /* October 12, 2006; tuned December 16, 2007; changed November 01, 2008,
  optimised January 15, 2011; changed December 27, 2014 */
  size_t n;
  const char *s = agn_checklstring(L, 1, &n);  /* 2.39.12 embedded zero fix */
  if (n == 0) {  /* 2.3.4 fix */
    lua_pushfalse(L);
    return 1;
  }
  for ( ; n--; s++) {
    if (*s && (tools_alphadia[uchar(*s) + 1] & (__ALPHA | __DIACR)) ) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);  /* 4.11.4 fix */
  return 1;
}


static int str_isvowel (lua_State *L) {  /* based on str_ismagic, 4.11.4 */
  size_t n;
  const char *s = agn_checklstring(L, 1, &n);  /* 2.39.12 embedded zero fix */
  int withy = agnL_optboolean(L, 2, 1);
  if (n == 0) {
    lua_pushfalse(L);
    return 1;
  }
  for ( ; n--; s++) {
    if (*s && !tools_isvowel(uchar(*s), withy)) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_isconsonant (lua_State *L) {  /* based on str_ismagic, 4.11.4 */
  size_t n;
  const char *s = agn_checklstring(L, 1, &n);  /* 2.39.12 embedded zero fix */
  if (n == 0) {  /* 2.3.4 fix */
    lua_pushtrue(L);
    return 1;
  }
  for ( ; n--; s++) {
    if (*s && !tools_isconsonant(uchar(*s))) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_islatin (lua_State *L) {  /* October 12, 2006; tuned December 16, 2007 */
  size_t n, l;
  unsigned char token;
  const char *s = agn_checklstring(L, 1, &n);  /* 2.39.12 embedded zero fix */
  const char *p = luaL_optlstring(L, 2, NULL, &l);
  if (n == 0) {
    lua_pushfalse(L);
    return 1;
  }
  for ( ; n--; s++) {
    token = uchar(*s);
    if (*s && (token < 'a' || token > 'z') && (token < 'A' || token > 'Z')) {
      if (p && tools_memchr(p, uchar(*s), l)) continue;  /* 2.31.2 extension */
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_islatinnumeric (lua_State *L) {  /* based upon isAlphaNumeric; June 13, 2009 */
  size_t n, l;
  unsigned char token;
  const char *s = agn_checklstring(L, 1, &n);  /* 2.39.12 embedded zero fix */
  const char *p = luaL_optlstring(L, 2, NULL, &l);
  if (n == 0) {
    lua_pushfalse(L);
    return 1;
  }
  for ( ; n--; s++) {
    token = uchar(*s);
    if (*s && (token < 'a' || token > 'z') && (token < 'A' || token > 'Z') && ((token < '0' || token > '9'))) {
      if (p && tools_memchr(p, uchar(*s), l)) continue;  /* 2.31.2 extension */
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_isintegral (lua_State *L) {  /* based on deprecated str_isnumber, 3.17.1 */
  int base, checkforsign;
  size_t n, oldn;
  const char *s = agn_checklstring(L, 1, &n);
  checkforsign = agnL_optboolean(L, 2, 0);  /* 3.17.2 change */
  base = agnL_optposint(L, 3, 10);  /* 4.7.4 change */
  if (n == 0) {
    lua_pushfalse(L);
    return 1;
  }
  oldn = n - 1;
  for ( ; n--; s++) {
    if (*s && !(tools_isdigit(uchar(*s), base))) {
      if (checkforsign && oldn == n && oldn != 0 && (uchar(*s) == '+' || uchar(*s) == '-')) {
        checkforsign = 0;  /* first char is a sign, and size string > 1 */
      } else {
        lua_pushfalse(L);
        return 1;
      }
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_isfractional (lua_State *L) {  /* based on deprecated str_isfloat, 3.17.1 */
  size_t n, oldn;
  int flag, checkforsign, base;
  const char *s = agn_checklstring(L, 1, &n);
  struct lconv *cv = localeconv();
  char decpoint = (cv ? cv->decimal_point[0] : '.');
  checkforsign = agnL_optboolean(L, 2, 0);  /* 3.17.2 change */
  base = agnL_optposint(L, 3, 10);  /* 4.7.4 change */
  flag = 1;
  oldn = n - 1;
  for ( ; n--; s++) {
    if (*s && !(tools_isdigit(uchar(*s), base))) {  /* isdigit() is not faster */
      if (uchar(*s) == decpoint && flag && oldn != 0) {  /* a simple '.' is invalid */
        flag = 0;
      } else if (checkforsign && n == oldn && oldn != 0 && (uchar(*s) == '+' || uchar(*s) == '-')) {  /* first char is a sign, and size string > 1 */
        checkforsign = 0;  /* do nothing */
      } else {
        lua_pushfalse(L);
        return 1;
      }
    }
  }
  lua_pushboolean(L, flag == 0);
  return 1;
}


/* check for integers and floats with the decimal separator determined by the locale; optionally checks for sign */
static int str_isnumeric (lua_State *L) {  /* 26.08.2012, Agena 1.7.7, extended 3.17.1 */
  int i, sign = 0, r = 1, nargs = lua_gettop(L);
  if (nargs == 0) {
    luaL_error(L, "Error in " LUA_QS ": need at least one string.", "strings.isnumeric");
  }
  if (nargs > 1 && lua_isboolean(L, nargs)) {
    sign = lua_toboolean(L, nargs--);
  }
  for (i=0; i < nargs && r; i++) {
    r = tools_isnumericstring(agn_checkstring(L, i + 1), sign);
  }
  agn_pushboolean(L, r);
  return 1;
}


static FORCE_INLINE int aux_iscomplex (lua_State *L, int idx) {
  int cexception;
#ifndef PROPCMPLX
  agn_tocomplexx(L, idx, &cexception);
#else
  lua_Number c[2];
  agn_tocomplexx(L, idx, &cexception, c);
#endif
  return cexception == 0;
}

/* Checks whether all string arguments represent complex numbers that have the imaginary unit `I`
   at their end and returns `true` or `false`. In this context, valid complex numbers are `I`,
   `0*I`, `0+I`, `-1-I`, `+1-I`, etc. Invalid complex numbers are `1`, `I*0`, `1+I*(-1)`, etc. 6.5.3 */
static int str_iscomplex (lua_State *L) {
  const char *str;
  int i, r = 1, nargs = lua_gettop(L);
  if (nargs == 0) {
    luaL_error(L, "Error in " LUA_QS ": need at least one string.", "strings.iscomplex");
  }
  for (i=0; i < nargs && r; i++) {
    str = agn_checkstring(L, i + 1);
    if (tools_isnumericstring(str, 1)) {
      r = 0;
    } else {
      r = aux_iscomplex(L, i + 1);
    }
  }
  agn_pushboolean(L, r);
  return 1;
}


/* Checks whether one or more strings all represent real or complex numbers and returns `true` or `false`.
   For each string s in the argument list it actually performs the check:

      strings.isnumeric(s, true) or strings.iscomplex(s)

   as long as the result is `true`. 6.5.3 */
static int str_isnumber (lua_State *L) {
  const char *str;
  int i, r = 1, nargs = lua_gettop(L);
  if (nargs == 0) {
    luaL_error(L, "Error in " LUA_QS ": need at least one string.", "strings.isnumber");
  }
  for (i=1; i <= nargs && r; i++) {
    str = agn_checkstring(L, i);
    r = tools_isnumericstring(str, 1) || aux_iscomplex(L, i);
  }
  agn_pushboolean(L, r);
  return 1;
}


/* check for integers and floats with a decimal comma in string; optionally checks for sign */
static int str_iscenumeric (lua_State *L) {  /* 26.08.2012, Agena 1.7.7, extended 3.17.1 */
  size_t n, oldn;
  const char *s = agn_checklstring(L, 1, &n);
  int checkforsign = agnL_optboolean(L, 2, 0);
  int flag = 1;
  if (n == 0) {
    lua_pushfalse(L);
    return 1;
  }
  oldn = n - 1;
  for ( ; n--; s++) {
    if (*s && (uchar(*s) < '0' || uchar(*s) > '9')) {  /* isdigit() is not faster */
      if (checkforsign && oldn == n && oldn != 0 && (uchar(*s) == '+' || uchar(*s) == '-')) {
        checkforsign = 0;  /* first char is a sign, and size string > 1, 3.17.1 */
      } else if (flag && oldn != 0 && uchar(*s) == ',') {  /* 3.17.1 a little tweak */
        flag = 0;
      } else {
        lua_pushfalse(L);
        return 1;
      }
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_isnumberspace (lua_State *L) {  /* June 29, 2007; tuned December 16, 2007 */
  size_t n;
  const char *s, *olds;
  int elaborate;
  s = agn_checklstring(L, 1, &n);
  elaborate = agnL_optboolean(L, 2, 0);
  for ( ; *s && uchar(*s) == ' ' && n--; s++) { };  /* `remove` leading spaces */
  if (n == 0) {
    lua_pushfalse(L);
    return 1;
  }
  olds = s;
  s += n - 1;
  for ( ; *s && uchar(*s) == ' ' && n--; s--) { };  /* `remove` trailing spaces */
  s = olds;
  for ( ; n--; s++) {
    if (*s && ((uchar(*s) < '0' || uchar(*s) > '9') && !(!elaborate && uchar(*s) == ' '))) {  /* isdigit() is not faster */
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_isspace (lua_State *L) {  /* Agena 1.8.0, September 25, 2012 */
  size_t l;
  const char *s = agn_checklstring(L, 1, &l);  /* 2.39.12 embedded zero fix */
  if (l == 0) {
    lua_pushfalse(L);
    return 1;
  }
  for ( ; l--; s++) {
    if (*s && uchar(*s) != ' ') {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_isisospace (lua_State *L) {  /* 31.12.2012 */
  const char *s = agn_checkstring(L, 1);
  if (*s == '\0') {
    lua_pushfalse(L);
    return 1;
  }
  for ( ; *s != '\0'; s++) {
    if (!isISOspace(uchar(*s))) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_isblank (lua_State *L) {  /* Agena 1.8.0, September 25, 2012; extended 2.3.4, December 27, 2014 */
  const char *s = agn_checkstring(L, 1);
  int option = agnL_optboolean(L, 2, 0);
  if (*s == '\0') {
    lua_pushfalse(L);
    return 1;
  }
  if (option) {
    for ( ; *s != '\0'; s++) {
      if (!(tools_charmap[uchar(*s) + 1] & __BLANK)) {
        lua_pushfalse(L);
        return 1;
      }
    }
  } else {
    for ( ; *s != '\0'; s++) {
      if (!(uchar(*s) == ' ' || uchar(*s) == '\t')) {
        lua_pushfalse(L);
        return 1;
      }
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_ishex (lua_State *L) {  /* 2.3.4, December 27, 2014, extended 3.17.1 */
  size_t n, oldn;
  const char *s = agn_checklstring(L, 1, &n);
  int checkforsign = agnL_optboolean(L, 2, 0);
  if (n == 0) {
    lua_pushfalse(L);
    return 1;
  }
  oldn = n - 1;
  for ( ; n--; s++) {
    if (checkforsign && oldn == n && oldn != 0 && (uchar(*s) == '+' || uchar(*s) == '-')) {
      checkforsign = 0;  /* first char is a sign, and size string > 1, 3.17.1 */
    } else if (!(tools_charmap[uchar(*s) + 1] & __HEXAD)) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_isalpha (lua_State *L) {  /* October 12, 2006, extended May 17, 2007; tuned December 16, 2007;
  changed November 01, 2008; changed December 27, 2014 */
  size_t n, l;
  const char *s = agn_checklstring(L, 1, &n);
  const char *p = luaL_optlstring(L, 2, NULL, &l);
  if (n == 0) {
    lua_pushfalse(L);
    return 1;
  }
  for ( ; n--; s++) {
    if (*s && (!(tools_alphadia[uchar(*s) + 1] & (__ALPHA | __DIACR))) ) {
      if (p && tools_memchr(p, uchar(*s), l)) continue;  /* 2.31.2 extension */
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_isspec (lua_State *L ) {  /* 1.10.6, 11.04.2013; 2.3.4, 26.12.2014 */
  size_t l;
  const char *s = agn_checklstring(L, 1, &l);
  if (*s == '\0') {
    lua_pushfalse(L);
    return 1;
  }
  for ( ; *s != '\0'; s++) {
    if (!(tools_charmap[uchar(*s) + 1] & __PUNCT)) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_iscontrol (lua_State *L ) {  /* 2.3.4, 27.12.2014; changed 2.16.1 */
  const char *s = agn_checkstring(L, 1);
  /* 2.16.1: we do not check for the empty string = \0, for \0 belongs to ASCII */
  for ( ; *s != '\0'; s++) {
    if (!(tools_charmap[uchar(*s) + 1] & __CNTRL)) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_isprintable (lua_State *L ) {  /* 2.3.4, 27.12.2014 */
  const char *s = agn_checkstring(L, 1);
  if (*s == '\0') {
    lua_pushfalse(L);
    return 1;
  }
  for ( ; *s != '\0'; s++) {
    if (!(tools_charmap[uchar(*s) + 1] & __PRINT)) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_isalphaspec (lua_State *L) {  /* 1.10.6, 11.04.2013; 2.3.4, 26.12.2014 */
  const char *s = agn_checkstring(L, 1);
  unsigned short int token = uchar(*s) + 1;
  if (*s == '\0') {
    lua_pushfalse(L);
    return 1;
  }
  for ( ; *s != '\0'; s++) {
    if (!( (tools_charmap[token] & __PUNCT) ||
           (tools_alphadia[token] & (__ALPHA | __DIACR)) ) ) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_isisoalpha (lua_State *L) {  /* December 31, 2012; no embedded zero check for \0 is an UTF-8 anchor */
  const char *s = agn_checkstring(L, 1);
  if (*s == '\0') {
    lua_pushfalse(L);
    return 1;
  }
  for ( ; *s != '\0'; s++) {
    if (!isISOalpha(*s)) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_isalphaspace (lua_State *L) {  /* June 28, 2007; tuned December 16, 2007; changed 2.3.4, 26.12.2014 */
  size_t n, l;
  unsigned short int token;
  const char *s = agn_checklstring(L, 1, &n);  /* 2.39.12 embedded zero fix */
  const char *p = luaL_optlstring(L, 2, NULL, &l);
  if (n == 0) {
    lua_pushfalse(L);
    return 1;
  }
  for ( ; n--; s++) {
    token = uchar(*s);
    if ( *s && (!( (tools_alphadia[token + 1] & ( __ALPHA | __DIACR)) || token == ' ' )) ) {
      if (p && tools_memchr(p, uchar(*s), l)) continue;  /* 2.31.2 extension */
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_islowerlatin (lua_State *L) {  /* April 30, 2007; tuned December 16, 2007 */
  size_t n, l;
  const char *s = agn_checklstring(L, 1, &n);  /* 2.39.12 embedded zero fix */
  const char *p = luaL_optlstring(L, 2, NULL, &l);
  if (n == 0) {
    lua_pushfalse(L);
    return 1;
  }
  for ( ; n--; s++) {
    if (*s && (uchar(*s) < 'a' || uchar(*s) > 'z')) {
      if (p && tools_memchr(p, uchar(*s), l)) continue;  /* 2.31.2 extension */
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_isloweralpha (lua_State *L) {  /* November 09, 2008; changed December 27, 2014 */
  size_t n, l;
  const char *s = agn_checklstring(L, 1, &n);
  const char *p = luaL_optlstring(L, 2, NULL, &l);
  if (n == 0) {
    lua_pushfalse(L);
    return 1;
  }
  for ( ; n--; s++) {
    if (*s && !(tools_charmap[uchar(*s) + 1] & __LOWER) ) {
      if (p && tools_memchr(p, uchar(*s), l)) continue;  /* 2.31.2 extension */
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_isisolower (lua_State *L) {  /* December 31, 2012; no embedded zero fix for \0 is an UTF-8 anchor */
  const char *s = agn_checkstring(L, 1);
  if (*s == '\0') {
    lua_pushfalse(L);
    return 1;
  }
  for ( ; *s != '\0'; s++) {
    if (!isISOlower(*s)) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_isupperlatin (lua_State *L) {  /* 0.21.0, April 19, 2009 */
  size_t n, l;
  const char *s = agn_checklstring(L, 1, &n);
  const char *p = luaL_optlstring(L, 2, NULL, &l);
  if (n == 0) {
    lua_pushfalse(L);
    return 1;
  }
  for ( ; n--; s++) {
    if (*s && (uchar(*s) < 'A' || uchar(*s) > 'Z')) {  /* 2.39.12 embedded zero fix */
      if (p && tools_memchr(p, uchar(*s), l)) continue;  /* 2.31.2 extension */
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_isupperalpha (lua_State *L) {  /* 0.21.0, April 19, 2009; 2.3.4 December 27, 2014 */
  size_t n, l;
  const char *s = agn_checklstring(L, 1, &n);  /* 2.39.12 embedded zero fix */
  const char *p = luaL_optlstring(L, 2, NULL, &l);
  if (n == 0) {
    lua_pushfalse(L);
    return 1;
  }
  for ( ; n--; s++) {
    if (*s && !(tools_charmap[uchar(*s) + 1] & __UPPER) ) {
      if (p && tools_memchr(p, uchar(*s), l)) continue;  /* 2.31.2 extension */
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_isisoupper (lua_State *L) {  /* 31.12.2012; no embedded zero fix for \0 is an UTF-8 anchor */
  const char *s = agn_checkstring(L, 1);
  if (*s == '\0') {
    lua_pushfalse(L);
    return 1;
  }
  for ( ; *s != '\0'; s++) {
    if (!isISOupper(*s)) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int str_isalphanumeric (lua_State *L) {  /* October 12, 2006; extended May 17, 2007; tuned December 16, 2007; 2.3.4 December 27, 2014 */
  size_t n, l;
  unsigned short int token;
  const char *s = agn_checklstring(L, 1, &n);
  const char *p = luaL_optlstring(L, 2, NULL, &l);
  if (n == 0) {
    lua_pushfalse(L);
    return 1;
  }
  for ( ; n--; s++) {
    token = uchar(*s);
    if ( token && ( !( (tools_alphadia[token + 1] & ( __ALPHA | __DIACR )) || (token >= '0' && token <= '9') )) ) {
      if (p && tools_memchr(p, uchar(*s), l)) continue;  /* 2.31.2 extension */
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


/* Checks whether the given string s consists entirely of ligatures and diacratics and returns
   true or false. The function works correctly with the ISO/IEC 8859-1 character set only. */
static int str_isdia (lua_State *L) {  /* based on str_isspace and str_isalpha; 2.10.0 */
  const char *s = agn_checkstring(L, 1);
  int atleastone = agnL_optboolean(L, 2, 0);
  if (*s == '\0') {
    lua_pushfalse(L);
    return 1;
  }
  if (atleastone) {
    for ( ;*s != '\0'; s++) {
      if (tools_alphadia[uchar(*s) + 1] & (__DIACR)) {
        lua_pushtrue(L);
        return 1;
      }
    }
    lua_pushfalse(L);
  } else {
    for ( ;*s != '\0'; s++) {
      if ( !(tools_alphadia[uchar(*s) + 1] & (__DIACR)) ) {
        lua_pushfalse(L);
        return 1;
      }
    }
    lua_pushtrue(L);
  }
  return 1;
}


#define aux_charmap_setupseq(L,chartype) { \
  lua_pushstring(L, (chartype)); \
  agn_createseq(L, 16); \
  lua_rawset(L, -3); \
}

#define aux_charmap_setuptbl(L,chartype) { \
  lua_pushstring(L, (chartype)); \
  lua_createtable(L, 255, 1); \
  lua_rawset(L, -3); \
}

#define aux_charmap_inserttoken(L,chartype,i) { \
  lua_getfield(L, -1, (chartype)); \
  lua_pushchar(L, i); \
  lua_seqinsert(L, -2); \
  agn_poptop(L); \
}


#define aux_charmap_setasciitbl(L,chartype,i) { \
  lua_getfield(L, -1, (chartype)); \
  lua_pushchar(L, i); \
  lua_rawseti(L, -2, i); \
  agn_poptop(L); \
}



/* Queries the internal tables to classify characters. Returns a table of key~value pairs: */
static int str_charmap (lua_State *L ) {  /* 2.10.0 */
  size_t i, j;
  lua_createtable(L, 0, 12);
  aux_charmap_setupseq(L, "alpha");
  aux_charmap_setupseq(L, "vowel");
  aux_charmap_setupseq(L, "dia");
  aux_charmap_setupseq(L, "control");
  aux_charmap_setupseq(L, "blank");
  aux_charmap_setupseq(L, "print");
  aux_charmap_setupseq(L, "punct");
  aux_charmap_setupseq(L, "digit");
  aux_charmap_setupseq(L, "hex");
  aux_charmap_setupseq(L, "upper");
  aux_charmap_setupseq(L, "lower");
  aux_charmap_setuptbl(L, "ascii");
  for (i=0; i < 256; i++) {
    j = i + 1;
    /* consciously fall-through ! */
    if ( tools_alphadia[j] & (__ALPHA) )
      aux_charmap_inserttoken(L, "alpha", i);
    if ( tools_alphadia[j] & (__VOWEL) )
      aux_charmap_inserttoken(L, "vowel", i);
    if ( tools_alphadia[j] & (__DIACR) )
      aux_charmap_inserttoken(L, "dia", i);
    if (tools_charmap[j]   & (__CNTRL) )
      aux_charmap_inserttoken(L, "control", i);
    if (tools_charmap[j]   & (__BLANK) )
      aux_charmap_inserttoken(L, "blank", i);
    if (tools_charmap[j]   & (__PRINT) )
      aux_charmap_inserttoken(L, "print", i);
    if (tools_charmap[j]   & (__PUNCT) )
      aux_charmap_inserttoken(L, "punct", i);
    if (tools_charmap[j]   & (__DIGIT) )
      aux_charmap_inserttoken(L, "digit", i);
    if (tools_charmap[j]   & (__HEXAD) )
      aux_charmap_inserttoken(L, "hex", i);
    if (tools_charmap[j]   & (__UPPER) )
      aux_charmap_inserttoken(L, "upper", i);
    if (tools_charmap[j]   & (__LOWER) )
      aux_charmap_inserttoken(L, "lower", i);
    aux_charmap_setasciitbl(L, "ascii", i);  /* 2.32.1 */
  }
  return 1;
}


static int str_isisoprint (lua_State *L) {  /* 31.12.2012 */
  const char *s = agn_checkstring(L, 1);
  if (*s == '\0') {
    lua_pushfalse(L);
    return 1;
  }
  for ( ; *s != '\0'; s++) {
    if (!isISOprint(uchar(*s))) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


/* Checks whether a string consists of glyphs only. It is a direct port to the C function `isgraph`. 2.16.1 */
static int str_isgraph (lua_State *L ) {
  const char *s = agn_checkstring(L, 1);
  if (*s == '\0') {
    lua_pushfalse(L);
    return 1;
  }
  for ( ; *s != '\0'; s++) {
    if (!isgraph(uchar(*s))) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


/* Checks whether a string consists entirely of unsigned char 7-bit characters only that fit into the
   UK/US character set. It is a direct port to the C function `isascii`. 2.16.1 */
static int str_isascii (lua_State *L ) {
  size_t l;
  const char *s = agn_checklstring(L, 1, &l);  /* 2.39.12 embedded zero fix */
  /* we do not check for the empty string = \0, for \0 belongs to ASCII */
  for ( ; l--; s++) {
    if (*s && !isascii(uchar(*s))) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


/* Returns the first index of a match, counting from 0, and the length of the match. */
const char *aux_match (lua_State *L, const char *s, const char *p, size_t l1, ptrdiff_t init, size_t *len) {
  MatchState ms;
  int anchor = (*p == '^') ? (p++, 1) : 0;
  const char *s1 = s + init;
  initMatchState(L, &ms, 0, s, l1);
  do {
    const char *res;
    ms.level = 0;
    if ( (res = match(&ms, s1, p, 1)) != NULL ) {
      ptrdiff_t start, end;
      start = s1 - s + 1;
      end = res - s;
      *len = end - start + 1;
      return s1;
    }
  } while (s1++ < ms.src_end && !anchor);
  *len = 0;
  return NULL;
}

/* Returns the length of a match. The function pushes nothing. Multiple captures seem to be quite rare, so we just issue an error
   in this case. 2.28.5 */
static size_t aux_pushonecapture (lua_State *L, MatchState ms, char *s1, const char *res, const char *procname) {
  size_t len;
  int nrets;
  luaL_checkstack(L, 1, "too many chunks");
  nrets = push_captures(&ms, s1, res);
  if (nrets != 1) {
    lua_pop(L, nrets);
    luaL_error(L, "Error in " LUA_QS ": got %d captures, i.e. more than one.", procname, nrets);
    return 0;
  }
  lua_tolstring(L, -1, &len);
  lua_pop(L, nrets);
  return len;
}

/* Searches for and trims a string using a pattern of _more_ than one character. We have both simple matching and patchern matching.
   `type` depicts the function that calls the function:
   type = 0 -> strings.ltrim, type = 1 -> strings.lrtrim, type = 2 -> strings.rtrim
   There must be at least two values on the _argument_ stack of the function calling aux_trimpattern ! 2.28.5 */
static void aux_trimpattern (lua_State *L, const char *s, const char *p, size_t ls, size_t lp, const char *procname, int type) {
  if (lua_toboolean(L, 3) ||  /* explicit request ? */
    !tools_hasstrchr(p, SPECIALS, lp)) {  /* or no special characters ? -> simple matching */
    /* do a plain search */
    char *olds;
    const char *s1;
    if (type < 2) {
      s1 = tools_lmemfind(s, ls, p, lp);
      if (s1 && s1 - s == 0) {  /* match at the start ? */
        s += lp; ls -= lp;  /* skip leading match */
      }
    }
    olds = (char *)s;
    if (type > 0 && *s) {  /* s must not be terminating \0 as we would move s beyond <eos> !!! */
      s += ls - lp;  /* move to end of string */
      s1 = tools_lmemfind(s, lp, p, lp);
      if (s1 && s1 - s == 0) ls -= lp;  /* match at the end */
    }
    lua_pushlstring(L, olds, ls);
  } else {  /* pattern matching */
    MatchState ms;
    size_t len;
    int dontskip2ndtry, hit;
    int anchor = (*p == '^') ? (p++, 1) : 0;
    char *s1, *s2, *olds;
    const char *res;
    s1 = (char *)s;
    olds = (char *)s;  /* 2.39.10 fix */
    initMatchState(L, &ms, 0, s, ls);
    dontskip2ndtry = 1;
    hit = 0;
    do {  /* try to find pattern at the start */
      ms.level = 0;
      if ((res = match(&ms, (const char *)s1, p, 1)) != NULL) {
        len = aux_pushonecapture(L, ms, s1, res, procname);
        if (type < 2 && res - len == s) {  /* match at the start ? */
          s1 = (char *)res;                /* wind forward */
          hit = 1;
        } else if (type > 0 && res == ms.src_end) {  /* match at the end ? */
          s1 = (char *)s;
          dontskip2ndtry = 0;
          hit = 1;
        }
        ls -= (hit == 1)*len;
        break;
      }
    } while (s1++ < ms.src_end && !anchor);  /* we have to explicitly increment s1, otherwise `match` cannot find a pattern ! */
    if (!hit && type < 2) {  /* no match at all ? */
      lua_pushlstring(L, s, ls);
      return;
    }
    if (type > 0 && dontskip2ndtry) {  /* do not run twice as we already have found the pattern at the end of string */
      int i;
      char pm[lp + 2];  /* we use a variable-length array (VLA) for match() may throw errors later; 6.1.1 patch */
      pm[lp] = 0; pm[lp + 1] = 0;
      for (i=0; i < lp; i++) pm[i] = p[i];
      if (p[lp - 1] != '$') pm[lp] = '$';
      s2 = (hit) ? s1 : olds;  /* 2.39.10 fix */
      initMatchState(L, &ms, 0, s2, ls);
      do {  /* try to find pattern at the end */
        ms.level = 0;
        if ((res = match(&ms, (const char *)s2, pm, 1)) != NULL) {
          ls -= aux_pushonecapture(L, ms, s2, res, procname);
          break;
        }
      } while (s2++ < ms.src_end && !anchor);  /* we have to explicitly increment s2, otherwise `match` cannot find a pattern ! */
      if (!hit) s1 = olds;    /* 2.39.10 fix */
    }
    lua_pushlstring(L, s1, ls);
  }
}

/* 1.11.2, delete leading and trailing white spaces or the given given leading o trailing character or string */
static int str_lrtrim (lua_State *L) {
  const char *s, *p;
  size_t ls, lp;
  s = agn_checklstring(L, 1, &ls);
  p = luaL_optlstring(L, 2, " ", &lp);
  if (lp == 1) {
    while (*s == *p) { s++; ls--; };  /* remove leading spaces */
    while (ls > 0 && uchar(s[ls - 1]) == *p) ls--;  /* remove trailing spaces */
    lua_pushlstring(L, s, ls);
  } else {  /* 2.28.5 extension */
    aux_trimpattern(L, s, p, ls, lp, "strings.lrtrim", 1);
  }
  return 1;
}


static int str_rtrim (lua_State *L) {
  size_t ls, lp;
  const char *s = agn_checklstring(L, 1, &ls);
  const char *p = luaL_optlstring(L, 2, " ", &lp);
  if (lp == 1) {
    while (ls > 0 && uchar(s[ls - 1]) == *p) ls--;
    lua_pushlstring(L, s, ls);
  } else {  /* 2.28.5 extension */
    aux_trimpattern(L, s, p, ls, lp, "strings.rtrim", 2);
  }
  return 1;
}


static int str_ltrim (lua_State *L) {
  size_t ls, lp;
  const char *s = agn_checklstring(L, 1, &ls);
  const char *p = luaL_optlstring(L, 2, " ", &lp);
  /* 0.13.4 patch; do not optimize `while (*s == ' ') s++` to `while (*s++ == ' ')` ! */
  if (lp == 1) {
    while (*s == *p) s++;
    lua_pushstring(L, s);
  } else {  /* 2.28.5 extension */
    aux_trimpattern(L, s, p, ls, lp, "strings.ltrim", 0);
  }
  return 1;
}


/* Right-padding, 7.7.8, created with the help of Gemini AI */
static int str_ljustify (lua_State *L) {
  size_t s_len = 0;
  size_t filler_len = 0;
  const char *s = luaL_checklstring(L, 1, &s_len);
  size_t width = agn_checknonnegint(L, 2);
  const char *filler = luaL_optlstring(L, 3, " ", &filler_len);
  if (s_len >= width) {
    lua_pushlstring(L, s, width);
    return 1;
  }
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  luaL_addlstring(&b, s, s_len);
  size_t bytes_needed = width - s_len;
  while (bytes_needed > 0) {
    if (bytes_needed >= filler_len) {
      luaL_addlstring(&b, filler, filler_len);
      bytes_needed -= filler_len;
    } else {
      luaL_addlstring(&b, filler, bytes_needed);
      bytes_needed = 0;
    }
  }
  luaL_pushresult(&b);
  return 1;
}


/* Leftt-padding, 7.7.8, created with the help of Gemini AI */
static int str_rjustify (lua_State *L) {
  size_t s_len = 0;
  size_t filler_len = 0;
  const char *s = luaL_checklstring(L, 1, &s_len);
  size_t width = agn_checknonnegint(L, 2);
  const char *filler = luaL_optlstring(L, 3, " ", &filler_len);
  if (s_len >= width) {
    lua_pushlstring(L, s + (s_len - width), width);
    return 1;
  }
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  size_t bytes_needed = width - s_len;
  while (bytes_needed > 0) {
    if (bytes_needed >= filler_len) {
      luaL_addlstring(&b, filler, filler_len);
      bytes_needed -= filler_len;
    } else {
      luaL_addlstring(&b, filler + (filler_len - bytes_needed), bytes_needed);
      bytes_needed = 0;
    }
  }
  luaL_addlstring(&b, s, s_len);
  luaL_pushresult(&b);
  return 1;
}


static int str_isolower (lua_State *L) {  /* 31.12.2012 */
  luaL_Buffer b;
  size_t l;
  const char *s = luaL_checklstring(L, 1, &l);  /* 2.39.12 embedded zero fix */
  luaL_buffinit(L, &b);
  while (l--) { luaL_addchar(&b, toISOlower(*s)); s++; };
  luaL_pushresult(&b);
  return 1;
}


static int str_isoupper (lua_State *L) {  /* 31.12.2012 */
  luaL_Buffer b;
  size_t l;
  const char *s = luaL_checklstring(L, 1, &l);  /* 2.39.12 embedded zero fix */
  luaL_buffinit(L, &b);
  while (l--) { luaL_addchar(&b, toISOupper(*s)); s++; }
  luaL_pushresult(&b);
  return 1;
}


static int str_tolower (lua_State *L) {  /* 2.26.1 */
  luaL_Buffer b;
  size_t l;
  const char *s = luaL_checklstring(L, 1, &l);  /* 2.39.12 embedded zero fix */
  luaL_buffinit(L, &b);
  if (lua_gettop(L) > 1) {
    /* *s | ('A' ^ 'a') actually changes many non-alphabetic chars, so we use a lookup */
    while (l--) { luaL_addchar(&b, tools_lowercase[uchar(*s++)]); };
  } else {
    while (l--) { luaL_addchar(&b, tolower(uchar(*s++))); };
  }
  luaL_pushresult(&b);
  return 1;
}


static int str_toupper (lua_State *L) {  /* 2.26.1 */
  luaL_Buffer b;
  size_t l;
  const char *s = luaL_checklstring(L, 1, &l);  /* 2.39.12 embedded zero fix */
  luaL_buffinit(L, &b);
  if (lua_gettop(L) > 1) {
    while (l--) { luaL_addchar(&b, tools_uppercase[uchar(*s++)]); }
  } else {
    while (l--) { luaL_addchar(&b, toupper(uchar(*s++))); };
  }
  luaL_pushresult(&b);
  return 1;
}


/* Applies a function f to the ASCII value of each character in string str and returns a new string. The function
   must return an integer in the range 0 .. 255, otherwise an error is issued. */

static int str_transform (lua_State *L) {  /* 31.12.2012 */
  luaL_Buffer b;
  const char *s;
  lua_Number x;
  int firstarg;
  firstarg = lua_type(L, 1);
  luaL_typecheck(L, firstarg == LUA_TFUNCTION, 1, "procedure expected", firstarg);
  luaL_checkstack(L, 2, "not enough stack space");  /* 2.31.6/7 fix */
  s = luaL_checkstring(L, 2);
  luaL_buffinit(L, &b);
  while (*s != '\0') {
    lua_pushvalue(L, 1);  /* push function */
    lua_pushinteger(L, *s);
    lua_call(L, 1, 1);
    if (!agn_isnumber(L, -1)) {
      luaL_clearbuffer(&b);  /* 2.14.9 fix */
      luaL_error(L, "Error in " LUA_QS ": function must return a number, got %s.", "strings.transform",
        luaL_typename(L, -1));
    }
    x = agn_tonumber(L, -1);
    agn_poptop(L);
    if (x < 0 || x > 255 || tools_isfrac(x)) {
      luaL_clearbuffer(&b);  /* 2.14.9 fix */
      luaL_error(L, "Error in " LUA_QS ": function must return an integer in 0 .. 255.", "strings.transform");
    }
    luaL_addchar(&b, x);
    s++;
  }
  luaL_pushresult(&b);
  return 1;
}


static int str_hits (lua_State *L) {
  size_t l1, l2;
  const char *s1;
  const char *src = agn_checklstring(L, 1, &l1);
  const char *p = agn_checklstring(L, 2, &l2);
  size_t n = 0;
  int init = 0;
  if (l2 == 0) {  /* else infinite loop */
    lua_pushnumber(L, 0);
    return 1;
  }
  if (lua_toboolean(L, 3) ||  /* Agena 1.3.2: explicit request? */
      !tools_hasstrchr(p, SPECIALS, l2)) {  /* or no special characters? 2.16.5/2.16.12/2.26.3 optimisation */
    while (1) {
      s1 = tools_lmemfind(src + init, l1 - init, p, l2);  /* 2.16.12 tweak */
      if (s1) {
        init += (s1 - (src + init)) + l2;
        n++;
      } else
        break;
    }
  } else {  /* Agena 1.3.2 */
    int anchor;
    MatchState ms;
    anchor = (*p == '^') ? (p++, 1) : 0;
    s1 = src;
    initMatchState(L, &ms, 0, src, l1);
    do {
      const char *res;
      ms.level = 0;
      if ((res = match(&ms, s1, p, 1)) != NULL) {
        s1 = (char *)res;  /* 2.28.5 fix */
        n++;
      }
    } while (s1++ < ms.src_end && !anchor);
  }
  lua_pushnumber(L, (lua_Number)n);
  return 1;
}


/* 0.14.0 as of April 08, 2009, inspired by the REXX function `words`; the Regina REXX interpreter
   features a very elegant and shorter C implementation, but it is not faster. */
static int str_words (lua_State *L) {
  size_t i, len;
  int c, flag;
  const char *s = agn_checklstring(L, 1, &len);
  const char *d = luaL_optstring(L, 2, " ");
  flag = lua_toboolean(L, 3);  /* changed 1.5.1: explicit request to ignore succeeding delimiters ? */
  i = c = 0;
  while (flag && *s == *d) { s++; i++; }  /* skip leading delimiters */
  if ((i == len - 1 && i != 0) || len == 0)  /* string consists entirely of delimiters or is empty ? */
    c = -1;  /* do not count anything */
  else {
    for (; len--; i++, s++) {  /* 2.39.12 embedded zero fix */
      if (*s && *s == *d) {
        if (flag && *(s + 1) != *d) { c++; continue; };
        if (!flag) c++;
      }
    }
    s--;  /* step back one character */
    /* if it ends with a delimiter, decrement c because it was already counted in the loop above */
    if (flag && *s == *d) c--;
  }
  lua_pushnumber(L, c + 1);
  return 1;
}


void aux_strdelete (lua_State *L, const char *src, const char *p, size_t l1, size_t l2, lua_Number n) {
  int init, pmatching;
  const char *s2;
  init = 0;  /* cursor */
  pmatching = tools_hasstrchr(p, SPECIALS, l2);  /* 4.10.8 */
  s2 = (!pmatching) ?         /* 2.16.5/2.16.12/2.26.3 optimisation */
    tools_lmemfind(src + init, l1 - init, p, l2) :   /* 2.16.12 tweak */
    aux_match(L, src + init, p, l1 - init, 0, &l2);  /* 2.12.0 RC 4 extension */
    /* aux_match() issues an error with malformed patterns at this point so we will have no memory leaks later */
  if (s2) {  /* any match ? */
    size_t c;
    Charbuf buf;
    charbuf_init(L, &buf, 0, l1, 1);
    c = 0;
    while (1) {
      if (init) /* do not conduct very first search again */
        s2 = (!pmatching) ? tools_lmemfind(src + init, l1 - init, p, l2) :   /* 2.16.12 tweak */
                            aux_match(L, src + init, p, l1 - init, 0, &l2);  /* 4.10.8 fix */
      if (s2 && c++ < n) {
        charbuf_append(L, &buf, src + init, s2 - (src + init));
        init += (s2 - (src + init)) + l2;
      } else {
        charbuf_append(L, &buf, src + init, l1 - init);
        break;
      }
    }
    charbuf_finish(&buf);  /* better sure than sorry, 2.39.12 */
    lua_pushlstring(L, buf.data, buf.size);
    charbuf_free(&buf);
  } else { /* no match */
    lua_pushlstring(L, src, l1);  /* remove original source unaltered */
  }
  /* pushes the shortened or original string onto the stack */
}

/* 0.20.0, inspired by the REXX function `delstr`; patched 1.10.0 */
static int str_remove (lua_State *L) {
  size_t pos, l1;
  long int offset, nchars;
  const char *src;
  src = agn_checklstring(L, 1, &l1);
  if (agn_isstring(L, 2)) {  /* 2.11.3 extension: cut out a substring */
    const char *p;
    size_t l2;
    int i, nargs, onestring;
    lua_Number n;
    nargs = lua_gettop(L);
    p = lua_tolstring(L, 2, &l2);
    if (l2 == 0) {
      lua_pushvalue(L, 1);
      return 1;
    }
    onestring = 0;
    if (nargs == 2) {
      n = HUGE_VAL; onestring = 1;
    } else if (agn_isnumber(L, 3)) {
      n = agn_tonumber(L, 3); onestring = 1;
    }
    if (onestring) {
      aux_strdelete(L, src, p, l1, l2, n);  /* pushes result onto the stack */
    } else {
      const char *newsrc = NULL;
      lua_pushstring(L, src);
      for (i=2; i <= nargs; i++) {
        if (!agn_isstring(L, i)) {
          agn_poptop(L);
          luaL_error(L, "Error in " LUA_QS ": argument #%d must be a string, got %s.", "strings.remove", i, luaL_typename(L, i));
        }
        p = lua_tolstring(L, i, &l2);
        if (l2 == 0) return 1;  /* return src */
        newsrc = lua_tolstring(L, -1, &l1);
        aux_strdelete(L, newsrc, p, l1, l2, HUGE_VAL);  /* pushes result onto the stack */
        lua_remove(L, -2);  /* remove original source from stack, leave shortened string at the stack top. */
      }
    }
  } else {
    pos = tools_posrelat(agnL_checkint(L, 2), l1) - 1;  /* 2nd argument is the position */
    nchars = agnL_optinteger(L, 3, 1);  /* number of characters to be deleted, Agena 1.10.0 */
    if (nchars < 1)
      luaL_error(L, "Error in " LUA_QS ": third argument must be positive.", "strings.remove");
    if (pos >= l1)  /* 7.9.5 change, no check for negative value any longer */
      luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "strings.remove", pos + 1);
    offset = pos + nchars;
    if (offset > l1) offset = l1;  /* avoid invalid accesses beyond the length of the string */
    /* directly writing the string is slower, e.g. using concat and substr(l) ! */
    lua_pushlstring(L, src, pos);  /* push left part of original string */
    lua_pushstring(L, src + offset);  /* push rest of original string */
    lua_concat(L, 2);
  }
  return 1;
}


/* 1.2.0, inspired by the REXX function `delstr` (i.e. its counterpart `delstr`). charbuf_append actually is 10% slower (checked twice).  */
static int str_include (lua_State *L) {
  size_t pos, origlen, newstrlen;
  const char *orig, *newstr;
  orig = agn_checklstring(L, 1, &origlen);
  pos = tools_posrelat(agnL_checkint(L, 2), origlen);  /* 2nd argument is the position */
  newstr = agn_checklstring(L, 3, &newstrlen);
  if (pos < 1 || pos > origlen + 1)  /* 1.5.0 patch */
    luaL_error(L, "in " LUA_QL("strings.include") ": index %d out of range.", pos);
  /* directly writing the string is slower, e.g. using concat and substr(l) ! */
  lua_pushlstring(L, orig, pos - 1);  /* push left part of original string */
  lua_pushlstring(L, newstr, newstrlen);
  lua_pushstring(L, orig + pos - 1);  /* push rest of original string */
  lua_concat(L, 3);
  return 1;
}


/* Check options for map, select, remove, subs; externalised 2.31.5 */
void aux_splitcheckoptions (lua_State *L, int idx,
  int *nargs, int fromnarg, int *init, int *convert, int *match, int *bailout, char **unwrap, char **delim,
    int *header, int *ignorefnidx, int *skipfaultylines, const char *procname) {
  int checkoptions;
  *init = 0;       /* start at first character */
  *match = 0;      /* 1 = apply pattern matching, 0 = do not */
  *unwrap = NULL;  /* do not remove enclosing characters */
  *bailout = 1;    /* bail out if an error occurs, 0 = do not, 1 = throw an exception */
  *header = 0;     /* 0 = no header */
  *ignorefnidx = 0;  /* stack index of the function used for ignore option */
  *skipfaultylines = 0;  /* 1 = in case of a non-existing field, do not return `fail` and proceed with the next line instead */
  checkoptions = 8;  /* check n options; CHANGE THIS if you add/delete options */
  if (*nargs >= fromnarg && lua_ispair(L, *nargs))  /* 3.15.2 change, only two instead of four elements will be pushed next */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= fromnarg && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq("convert", option)) {
        *convert = lua_isboolean(L, -1) ? lua_toboolean(L, -1) : *convert;  /* 5.5.1 fix */
      } else if (tools_streq("bailout", option)) {
        *bailout = lua_isboolean(L, -1) ? lua_toboolean(L, -1) : *bailout;  /* 5.5.1 fix */
      } else if (idx == 0 && tools_streq("match", option)) {
        *match = lua_isboolean(L, -1) ? lua_toboolean(L, -1) : *match;  /* 5.5.1 fix */
      } else if (tools_streq("init", option)) {
        *init = agn_isinteger(L, -1) ? agn_tointeger(L, -1) : *init;  /* 5.5.1 fix */
      } else if (idx && tools_streq("header", option)) {  /* 3.10.4 */
        *header = lua_isboolean(L, -1) ? lua_toboolean(L, -1) : *header;  /* 5.5.1 fix */
      } else if (idx && tools_streqx(option, "skipfaultylines", "skipfaulty", NULL)) {  /* 3.10.4 */
        *skipfaultylines = lua_isboolean(L, -1) ? lua_toboolean(L, -1) : *skipfaultylines;
      } else if (tools_streq(option, "unwrap") && agn_isstring(L, -1)) {  /* 5.5.1 fix */
        size_t l;
        char *r = (char *)lua_tolstring(L, -1, &l);
        if (*unwrap != NULL) xfree(*unwrap);
        *unwrap = tools_strdup(r);
        if (!unwrap)
          luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);  /* 7.7.7 fix */
        if (l == 0) {
          xfreeall(*unwrap, *delim);
          agn_poptoptwo(L);
          luaL_error(L, "Error in " LUA_QS ": string with " LUA_QS " option is empty.", procname, option);
        }
      } else if (tools_streq("delim", option) && agn_isstring(L, -1)) {  /* 5.5.1 fix */
        size_t l;
        char *r = (char *)lua_tolstring(L, -1, &l);
        if (*delim != NULL) xfree(*delim);
        *delim = tools_strdup(r);
        if (!delim)
          luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);  /* 7.7.7 fix */
        if (l == 0) {
          xfreeall(*unwrap, *delim);
          agn_poptoptwo(L);
          luaL_error(L, "Error in " LUA_QS ": string with " LUA_QS " option is empty.", procname, option);
        }
      } else if (idx && tools_streq("ignore", option) && (lua_isfunction(L, -1) || agn_isstring(L, -1))) {  /* 3.10.4, 5.5.1 fix */
        lua_replace(L, *nargs);  /* replace option at stack position *nargs with function or string */
        agn_poptop(L);  /* drop lhs of option */
        *ignorefnidx = (*nargs)--;  /* stack position of function */
        continue;  /* do not nil position */
      } else {
        agn_poptoptwo(L);
        xfreeall(*unwrap, *delim);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS " or right-hand side.", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    agn_poptoptwo(L);
    lua_pushnil(L);  /* instead nil the position of the option just processed */
    lua_replace(L, (*nargs)--);
  }
  if (*ignorefnidx) {  /* move function of ignore option to end of rewamped stack */
    lua_settop(L, idx + 1);
    lua_copy(L, *ignorefnidx, idx + 1);
    lua_pushnil(L);
    lua_replace(L, *ignorefnidx);
    *ignorefnidx = idx + 1;
  }
}

/* 1.5.1, 14.11.2011, extract the given fields in the given order from a string;
   extended 2.0.0 RC4 to support sequences */
#define setresult(L,idx,k,istab) { \
  if (istab) \
    lua_rawseti(L, idx, k); \
  else \
    lua_seqrawseti(L, idx, k); \
}

static int str_fields (lua_State *L) {
  const char *e, *s, *s1;
  char *unwrap, *sep;
  int isseq, istab, tonumber, match, nargs, bailout, init, header, ignorefnidx, skipfaultylines, noindex, pmatching,
      rc, checkintegerarg;
  size_t c, i, n, l1, l2, lu;
  lua_Integer *inds, index;
  inds = NULL;
  unwrap = sep = NULL;
  c = n = tonumber = noindex = lu = 0;  /* position of token in string */
  header = ignorefnidx = skipfaultylines = 0;
  nargs = lua_gettop(L);
  s = agn_checklstring(L, 1, &l1);
  sep = tools_strdup(" ");
  if (!sep)  /* 7.7.7 fix */
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "strings.field");
  aux_splitcheckoptions(L, 0, &nargs, 2, &init, &tonumber, &match, &bailout, &unwrap, &sep,
    &header, &ignorefnidx, &skipfaultylines, "strings.fields");  /* 2.39.1 */
  init = 0;  /* 4.10.8 fix */
  if (lua_istrue(L, nargs)) { nargs--; tonumber = 1; }
  if (nargs > 1 && agn_isstring(L, nargs)) {
    xfree(sep);
    sep = tools_strdup(agn_tostring(L, nargs--));
    if (!sep)
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "strings.field");
  }
  /* start the processing */
  l2 = tools_strlen(sep);  /* 2.17.8 tweak */
  if (!agnL_isvalidpattern(L, sep, l2, &pmatching, 1)) {  /* 6.1.1 fix, invalid pattern given */
    xfreeall(unwrap, sep);
    luaL_error(L, "Error in " LUA_QS ": %s", "strings.fields", agn_tostring(L, -1));
  }
  isseq = lua_isseq(L, 2);  /* 2.0.0 RC4 */
  istab = lua_istable(L, 2);  /* 4.10.8 */
  nargs = (isseq || istab) ? agn_nops(L, 2) : nargs - 1;  /* nargs now has the number of fields to be retrieved */
  noindex = nargs == 0;  /* no index given ? -> return all fields */
  if (l2 == 0 || tools_streq(s, sep)) {
    lua_pushvalue(L, 1);  /* 2.39.1 fix */
    xfreeall(unwrap, sep);  /* 5.5.1 fix */
    return 1;  /* delimiter is the empty string or delimiter and string are equal ?  2.16.12 tweak */
  }
  do {  /* determine the number of words in the string */
    s1 = (pmatching == 0) ? tools_lmemfind(s + init, l1 - init, sep, l2) :  /* 2.16.12 tweak; 4.10.8 extension */
                            aux_match(L, s + init, sep, l1 - init, 0, &l2);  /* l2 contains the length of the match */
    if (s1) {
      init += (s1 - (s + init)) + l2;
      n++;
    }
  } while (s1);
  n++;  /* now we have the number of fields, not separators */
  if (noindex) nargs = n;  /* retrieve all fields */
  if (istab)
    lua_createtable(L, nargs, 0);
  else
    agn_createseq(L, nargs);
  checkintegerarg = !noindex && !isseq && !istab;
  /* put all indices passed in the call into an array (this may not be an elegant implementation but it saves memory) */
  inds = agn_malloc(L, nargs*sizeof(lua_Integer), "strings.fields", sep, unwrap, NULL);  /* 1.9.1, 3.10.2/3 fix */
  for (i=1; i <= nargs; i++) {
    if (checkintegerarg && !agn_isinteger(L, i + 1)) {  /* 7.3.5 fix */
      agn_poptop(L);  /* drop sequence or table with intermediate empty strings */
      xfreeall(inds, unwrap, sep);
      if (i + 1 == 2)
        luaL_error(L, "Error in " LUA_QS ": argument #%d is not an integer, table or sequence.", "strings.fields", i + 1);
      else
        luaL_error(L, "Error in " LUA_QS ": argument #%d is not an integer.", "strings.fields", i + 1);
    }
    index = (noindex) ? i : ((isseq) ? agn_seqrawgetinumber(L, 2, i, &rc) :
                            ((istab) ? agn_getinumber(L, 2, i) : agn_tointeger(L, i + 1)));
    if (index < 0) index += n + 1;
    if (index <= 0 || index > n) {
      xfreeall(inds, unwrap, sep);  /* 2.37.7 */
      agn_poptop(L);  /* drop sequence or table with intermediate empty strings, 7.3.5 fix */
      if (bailout) {
        luaL_error(L, "in " LUA_QS ": index %d out of range.", "strings.fields", index);
      } else {  /* return the empty sequence just pushed onto the stack, 3.10.2 change */
        lua_pushfail(L);
        return 1;
      }
    }
    inds[i - 1] = index;
    lua_pushstring(L, "");  /* fill sequence with empty strings for it might later be filled in non-ascending field order */
    setresult(L, -2, i, istab);
  }
  if (unwrap) lu = tools_strlen(unwrap);  /* 3.10.3 tweak */
  /* collect all matching tokens into a sequence */
  if (!pmatching) {
    while ((e=strstr(s, sep)) != NULL) {
      c++;
      for (i=0; i < nargs; i++) {
        if (inds[i] == c) {  /* the user might pass the field numbers in arbitrary order */
          lua_pushlstring(L, s, e - s);
          if (unwrap) agnL_strunwrap(L, -1, unwrap, lu);
          if (tonumber) {
            if (!agnL_strtonumber(L, -1)) agnL_strtocomplex(L, -1);  /* 3.4.9 extension, complex value must be in the form a+I*b. */
          }
          setresult(L, -2, i + 1, istab);
        }
      }
      s = e + l2;
    }
  } else {  /* 4.10.8, match pattern in delimiter */
    init = 0;
    while ((e=aux_match(L, s + init, sep, l1 - init, 0, &l2)) != NULL) {
      c++;
      for (i=0; i < nargs; i++) {
        if (inds[i] == c) {  /* the user might pass the field numbers in arbitrary order */
          lua_pushlstring(L, s + init, e - (s + init));
          if (unwrap) agnL_strunwrap(L, -1, unwrap, lu);
          if (tonumber) {
            if (!agnL_strtonumber(L, -1)) agnL_strtocomplex(L, -1);  /* 3.4.9 extension, complex value must be in the form a+I*b. */
          }
          setresult(L, -2, i + 1, istab);
        }
      }
      init = e - s + l2;
    }
    s += init;
  }
  /* process last token */
  if (tools_strlen(s) != 0) {  /* not the empty string ?  2.17.8 tweak */
    c++;
    for (i=0; i < nargs; i++) {
      if (inds[i] == c) {  /* the user might pass the field numbers in arbitrary order */
        lua_pushstring(L, s);
        if (unwrap) agnL_strunwrap(L, -1, unwrap, lu);
        if (tonumber) {  /* 3.10.3 fix for complex numbers */
          if (!agnL_strtonumber(L, -1)) agnL_strtocomplex(L, -1);
        }
        setresult(L, -2, i + 1, istab);
      }
    }
  }
  /* now return the respective fields */
  xfreeall(inds, unwrap, sep);
  return 1;
}


/* Source: http://www.merriampark.com/ldjava.htm, modified; 2.10.0 */

/* similarity = 1: compute Damerau-Levenshtein similarity
   n: size of s [sic !]
   m: size of t
   dleven = 1: compute Damerau-Levenshtein distance; dleven = 0: compute Levenshtein distance

   Validated with the online calculators:
   DLD: https://tilores.io/damerau-Levenshtein-distance-with-adjacent-transpositions
   LD:  https://planetcalc.com/1721/

   Tuned by Gemini AI 7.8.11 by a factor of at least 37. */
static lua_Number aux_dleven (lua_State *L, const char *s, size_t n, const char *t, size_t m, int similarity, int dleven) {
  int i, j, cost, *p, *d, *dt, ux, uy, uz, ur;
  char cs, ct;
  lua_Number r;
  if (n*m == 0)  /* 3.10.1 extension */
    return (similarity) ? (n == 0 && m == 0) : AGN_NAN;
  if (n == m && memcmp(s, t, n) == 0) {
    return (similarity) ? 1.0 : 0.0;
  }
  p = NULL; d = NULL;
  size_t original_n = n;
  size_t original_m = m;
  const char *original_s = s;
  const char *original_t = t;
  size_t prefix_len = 0;
  while (prefix_len < n && prefix_len < m && s[prefix_len] == t[prefix_len]) prefix_len++;
  while (n > prefix_len && m > prefix_len && s[n - 1] == t[m - 1]) { n--; m--; }
  s += prefix_len; t += prefix_len;
  n -= prefix_len; m -= prefix_len;
  if (n == 0 || m == 0) {
    int final_dist = (n > m) ? n : m;
    if (similarity) {
      size_t final_max = (original_n > original_m) ? original_n : original_m;
      lua_Number weight = 1.0 - (lua_Number)final_dist/(lua_Number)final_max;
      if (weight > 0.7) {  /* adjust when up to the first 4 chars are in common */
        i = (prefix_len >= 4) ? 4 : prefix_len;
        if (i) weight += i * 0.1 * (1.0 - weight);
      }
      return weight;
    }
    return (lua_Number)final_dist;
  }
  if (n > m) {
    TOOLS_SWAP(s, t);
    TOOLS_SWAP(n, m);
  }
  p = agn_malloc(L, sizeof(int)*(n + 1), "strings.dleven", NULL);  /* previous cost array, horizontally */
  d = agn_malloc(L, sizeof(int)*(n + 1), "strings.dleven", p, NULL);  /* cost array, horizontally, 4.11.5 fix */
  for (i=0; i <= n; i++) p[i] = i;
  for (j=1; j <= m; j++) {
    ct = t[j - 1];
    d[0] = j;
    for (i=1; i <= n; i++) {
      cs = s[i - 1];
      cost = (cs != ct);
      if (dleven && cost && i > 1 && j > 1 && s[i - 2] == ct && t[j - 2] == cs)
        cost--;  /* with a transposition subtract, not add; modified */
      ux = d[i - 1] + 1;     /* 2.14.4, avoid double evaluation */
      uy = p[i] + 1;         /* ditto */
      uz = p[i - 1] + cost;  /* ditto */
      ur = fMin(ux, uy);     /* ditto */
      d[i] = fMin(ur, uz);
    }
    dt = p;
    p = d;
    d = dt;
  }
  if (similarity) {  /* 3.10.1 extension */
    /* fixed 7.8.11: Similarity scales are always based on the MAXIMUM length of the original inputs */
    size_t final_max = (original_n > original_m) ? original_n : original_m;
    lua_Number weight = 1.0 - (lua_Number)p[n]/(lua_Number)final_max;
    if (weight > 0.7) {  /* adjust when up to the first 4 chars are in common */
      j = (original_n >= 4) ? 4 : original_n;
      /* fixed 7.8.11: Common prefix bonus loop reads the original_s and original_t base pointers */
      for (i=0; (i < j && (original_s[i] == original_t[i])); i++);
      if (i) weight += i*0.1*(1.0 - weight);
    }
    r = weight;
  } else {
    r = (lua_Number)p[n];
  }
  xfreeall(p, d);
  return r;
}

static int str_dleven (lua_State *L) {
  size_t m, n;
  int similarity;
  const char *s, *t;
  s = agn_checklstring(L, 1, &n);
  t = agn_checklstring(L, 2, &m);
  similarity = agnL_optboolean(L, 3, 0);  /* 3.10.1 */
  lua_pushnumber(L, aux_dleven(L, s, n, t, m, similarity, 1));
  return 1;
}

/* Same as strings.dleven, but computes the Levenshtein distance or Levenshtein similarity of two strings, so
   strings.leven does not take transpositions into account. 4.10.9 */
static int str_leven (lua_State *L) {
  size_t m, n;
  int similarity;
  const char *s, *t;
  s = agn_checklstring(L, 1, &n);
  t = agn_checklstring(L, 2, &m);
  similarity = agnL_optboolean(L, 3, 0);
  lua_pushnumber(L, aux_dleven(L, s, n, t, m, similarity, 0));
  return 1;
}


/* hirschberg.c implements Hirschberg's algorithm for global string alignment in C.  To test it, compile it with
 * `c99 -o hirschberg hirschberg.c` and then run  ./hirschberg <string1> <string2>`. (hirschberg.c uses variable-
 *  length arrays, so the 99 standard is necessary.)
 *
 * Copyright (c) 2015 Lari Rasku. This code is released to the public domain, or under CC0 if not applicable.
 *
 * Taken from:
 * https://gist.githubusercontent.com/lxr/a222b1f063b974d88ce6/raw/cbd7df2e512cb1fc4bee1587707dd5bdcea657e8/hirschberg.c
 *
 * The algorithm implemented here yields a total structural space complexity of strictly (O(N + M)).
 */

/* A costfunc represents a cost scheme for Hirschberg's algorithm. It takes two characters and returns the cost of
   transforming the former to the latter. Insertions and deletions are encoded as transformations from and to
   null bytes. 7.8.10 */
typedef int (*costfunc)(char, char);

/* levenshtein is the common cost scheme for edit distance: matches cost nothing, while mismatches, insertions and
   deletions cost one each. */
static int levenshtein (char a, char b) {
  return a != b;
}

/* The recursive step in the Needleman-Wunsch algorithm involves choosing the cheapest operation out of three
   possibilities.  We store the costs of the three operations into an array and use the editop enum to index it. */
typedef enum { Del = 0, Sub = 1, Ins = 2, } editop;

static char  *hirschberg (const char *, const char*, costfunc);
static char  *hirschberg_recursive (char *, const char *, size_t, const char *, size_t, costfunc);
static char  *nwalign (char *, const char *, size_t, const char *, size_t, costfunc);
static void  nwlcost (int *, const char *, size_t, const char *, size_t, costfunc);
static void  nwrcost (int *, const char *, size_t, const char *, size_t, costfunc);
static editop  nwmin (int[3], char, char, costfunc);
static void  memrev (void *, size_t);
static void  *tryrealloc (void *, size_t);

/* hirschberg calculates the global alignment of a and b with the cost scheme f using Hirschberg's algorithm.
   It returns the string of the edit operations required to change a into b:

 *   +  insertion
 *  -  deletion
 *  =  match
 *  !  substitution

 The string should be free(3)'d when no longer needed.  On failure, hirschberg returns NULL and sets errno to
 ENOMEM.  errno may be set to ENOMEM even on a successful return if the area allocated for the alignment string
 could not be shrunk. */
static char *hirschberg (const char *a, const char *b, costfunc f) {
  char *c, *d;
  size_t m = strlen(a);
  size_t n = strlen(b);
  /* The alignment string of a and b is at most as long as the concatenation of the two (delete all of a + insert
     all of b). This can overshoot the actual length of the alignment string by quite a bit in many cases, so we
     try to shrink it with tryrealloc at the end of the function. */
  c = malloc((m + n + 1)*sizeof(char));
  if (c == NULL) return NULL;
  if (m > n) {
    /* hirschberg_recursive assumes that the first string passed to it is the shorter one, so if a is not, we
       flip it and b. The resulting alignment is otherwise equivalent to the non-flipped one, with the exception
       that insertion and deletion operators need to be flipped. */
    d = hirschberg_recursive(c, b, n, a, m, f);
    if (d == NULL) return NULL;
    c = tryrealloc(c, d - c + 1);
    for (d=c; *d != '\0'; d++) {
      switch (*d) {
        case '+': *d = '-'; break;
        case '-': *d = '+'; break;
      }
    }
    return c;
  }
  d = hirschberg_recursive(c, a, m, b, n, f);
  if (d == NULL) return NULL;
  return tryrealloc(c, d - c + 1);
}

/* hirschberg_recursive is the recursive part of Hirschberg's algorithm. The arguments are the same as hirschberg,
   with the exception that the length m of a and the length n of b are now explicitly passed, and c is a pointer
   to the buffer where the alignment string is to be written. hirschberg_recursive returns a pointer to the
   null byte written after the last alignment character. */
static char *hirschberg_recursive (char *c, const char *a, size_t m, const char *b, size_t n, costfunc f) {
  if (n > 1) {
    size_t i, mmid, nmid, array_elements;
    int *lcost, *rcost, *tcost, *heap_block;
    array_elements = m + 1;
    heap_block = (int *)malloc(3*array_elements*sizeof(int));
    if (heap_block == NULL) return NULL;
    lcost = heap_block;
    rcost = heap_block + array_elements;
    tcost = heap_block + (2*array_elements);
    nmid = n/2;
    nwlcost(lcost, a, m, b, nmid, f);
    nwrcost(rcost, a, m, b + nmid, n - nmid, f);
    mmid = 0;
    for (i = 0; i <= m; i++) {
      tcost[i] = lcost[i] + rcost[i];
      if (tcost[i] < tcost[mmid]) mmid = i;
    }
    c = hirschberg_recursive(c, a, mmid, b, nmid, f);
    c = hirschberg_recursive(c, a + mmid, m - mmid, b + nmid, n - nmid, f);
    free(heap_block);
    return c;
  } else {
    return nwalign(c, a, m, b, n, f);
  }
}

/* nwalign computes the Needleman-Wunsch alignment of a and b using the cost scheme f and writes it into
   the buffer c.  It returns a pointer to the null byte written after the last character in the alignment
   string. This function uses O(mn) space. hirschberg_recursive guarantees its own O(m) space usage by
   only calling this when n <= 1. */
static char *nwalign (char *c, const char *a, size_t m, const char *b, size_t n, costfunc f) {
  char *d;
  size_t i, j;
  int (*s)[n + 1];
  s = (int (*)[n + 1])malloc((m + 1)*sizeof(*s));
  if (s == NULL) return NULL;
  s[0][0] = 0;
  for (i=1; i <= m; i++) s[i][0] = s[i-1][0] + f(a[i-1], 0);
  for (j=1; j <= n; j++) s[0][j] = s[0][j-1] + f(0, b[j-1]);
  for (j=1; j <= n; j++) {
    for (i=1; i <= m; i++) {
      int cost[3] = { s[i-1][j], s[i-1][j-1], s[i][j-1] };
      s[i][j] = cost[nwmin(cost, a[i-1], b[j-1], f)];
    }
  }
  i = m; j = n; d = c;
  while (i > 0 && j > 0) {
    int cost[3] = { s[i-1][j], s[i-1][j-1], s[i][j-1] };
    switch (nwmin(cost, a[i-1], b[j-1], f)) {
    case Del:
      *d++ = '-'; i--;
      break;
    case Sub:
      *d++ = a[i-1] == b[j-1] ? '=' : '!'; i--; j--;
      break;
    case Ins:
      *d++ = '+'; j--;
      break;
    }
  }
  for (; i > 0; i--) *d++ = '-';
  for (; j > 0; j--) *d++ = '+';
  *d = '\0';
  memrev(c, d - c);
  free(s);
  return d;
}

/* nwlcost stores the last column of the Needleman-Wunsch alignment cost matrix of a and b into s. */
static void nwlcost (int *s, const char *a, size_t m, const char *b, size_t n, costfunc f) {
  size_t i, j;
  int ss, tmp;
  s[0] = 0;
  for (i=1; i <= m; i++) s[i] = s[i-1] + f(a[i-1], 0);
  for (j=1; j <= n; j++) {
    ss = s[0];
    s[0] += f(0, b[j-1]);
    for (i=1; i <= m; i++) {
      int cost[3] = { s[i-1], ss, s[i] };
      tmp = cost[nwmin(cost, a[i-1], b[j-1], f)];
      ss = s[i];
      s[i] = tmp;
    }
  }
}

/* nwrcost computes the reverse Needleman-Wunsch alignment of a and b, that is, matching their
   suffixes rather than prefixes.  The last column of this alignment cost matrix is stored into s. */
static void nwrcost (int *s, const char *a, size_t m, const char *b, size_t n, costfunc f) {
  ssize_t i, j;
  int ss, tmp;
  s[m] = 0;
  for (i=m - 1; i >= 0; i--) s[i] = s[i+1] + f(a[i], 0);
  for (j=n - 1; j >= 0; j--) {
    ss = s[m];
    s[m] += f(0, b[j]);
    for (i=m - 1; i >= 0; i--) {
      int cost[3] = { s[i+1], ss, s[i] };
      tmp = cost[nwmin(cost, a[i], b[j], f)];
      ss = s[i];
      s[i] = tmp;
    }
  }
}

/* nwmin returns the cheapest edit operation out of three possibilities when the "current" characters are
   a and b, the cost scheme is f, and the base costs of the Del, Sub, and Ins operations are recorded in
   the cost array in that order.  The cost array is modified by adding the edit costs for a and b to the
   appropriate cells. */
static editop nwmin (int cost[3], char a, char b, costfunc f) {
  size_t i;
  cost[Del] += f(a, 0);
  cost[Sub] += f(a, b);
  cost[Ins] += f(0, b);
  i = cost[Del] < cost[Sub] ? Del : Sub;
  i = cost[i] < cost[Ins] ? i : Ins;
  return i;
}

/* memrev reverses the size bytes pointed to by ptr. */
static void memrev (void *ptr, size_t size) {
  char *p = (char *)ptr;
  size_t i;
  char tmp;
  for (i=0; i < size/2; i++) {
    tmp = p[i];
    p[i] = p[size - 1 - i];
    p[size - 1 - i] = tmp;
  }
}

/* tryrealloc tries to resize the area pointed to by ptr to size. It returns the resized area on success
   and ptr on failure. On failure, errno is set. */
static void *tryrealloc (void *ptr, size_t size) {
  void *b = realloc(ptr, size);
  return b != NULL ? b : ptr;
}

/* The function takes two strings s and t and returns their global alignment in three results:
   the edit sequence, the alignment of s and the alignment of t.
   The function uses the Hirschberg algorithm which yields a total structural space complexity of strictly
   O(N + M).
   In the edit sequence, the first result, '=' stands for equal characters, '-' for deletion, '+' for
   insertion and '!' for different characters.
   See also: strings.diffs,  strings.dleven, strings.fuzzy, strings.leven. */

static int str_hirschberg (lua_State *L) {  /* 7.8.10 */
  char *align, *c;
  const char *s, *t;
  size_t n, m, eqs;
  Charbuf sbuf, tbuf;
  s = agn_checklstring(L, 1, &n);
  t = agn_checklstring(L, 2, &m);
  align = hirschberg(s, t, levenshtein);
  if (align == NULL)
    luaL_error(L, "Error in " LUA_QS ": could not process strings.", "strings.hirschberg");
  charbuf_init(L, &sbuf, 0, n, 1);
  charbuf_init(L, &tbuf, 0, m, 1);
  eqs = 0;
  for (c = align; *c != '\0'; c++) {  /* 7.8.11 tweak */
    switch (*c) {
      case '-': case '!':
        charbuf_appendchar(L, &sbuf, *s++);
        break;
      case '=':
        charbuf_appendchar(L, &sbuf, *s++);
        eqs++;
        break;
      default:
        charbuf_appendchar(L, &sbuf, ' ');
        break;
    }
    switch (*c) {
      case '+': case '!': case '=':
        charbuf_appendchar(L, &tbuf, *t++);
        break;
      default:
        charbuf_appendchar(L, &tbuf, ' ');
        break;
    }
  }
  charbuf_finish(&sbuf);
  charbuf_finish(&tbuf);
  luaL_checkstack(L, 5, "not enough stack space");
  lua_pushlstring(L, align, c - align);
  lua_pushlstring(L, sbuf.data, sbuf.size);
  lua_pushlstring(L, tbuf.data, tbuf.size);
  lua_pushnumber(L, c - align);
  lua_pushnumber(L, m == 0 || n == 0 ? AGN_NAN : c - align - eqs);
  charbuf_free(&sbuf);
  charbuf_free(&tbuf);
  xfree(align);
  return 5;
}


/* Computes the longest common subsequence (LCS) of two strings s, t, that is the longest subsequence of characters that is
   common to both s and t, provided that the characters are not required to occupy _consecutive_ positions. Note that the
   algorithm observes the order of characters, however. So if you swap one or more, you will get different results.

   The LCS allows for insertions and deletions, but not substitutions and transpositions.

   The returns are the length of the LCS plus the LCS itself.

   See also: strings.dice, strings.diffs, strings.dleven, strings.fuzzy, strings.jaro, strings.leven. 4.10.9

   The implementation has been tuned by Gemini AI by a factor of at least 37. 7.8.11.

   lcs := proc(s :: string, t :: string) is
      local m, n, i, j, l, r;
      m, n := size s, size t;
      c := tables.dimension(0:m, 0:n, 0);
      # Algorithm taken from https://en.wikipedia.org/wiki/Longest_common_subsequence
      for i to m do
         for j to n do
            if s[i] = t[j] then
               c[i, j] := c[i - 1, j - 1] + 1
            else
               c[i, j] := max(c[i, j - 1], c[i - 1, j])
            fi
         od
      od;
      # Algorithm taken from https://www.programiz.com/dsa/longest-common-subsequence
      stack.switchto('l');  # use the stack to save memory
      i, j, l := m, n, c[m, n];
      while i*j > 0 do
         if s[i] = t[j] then
            pushd s[i];
            i--; j--;
         elif c[i - 1, j] > c[i, j - 1] then
            i--
         else
            j--
         fi
      od;
      r := stack.dumpd(l, true);  # join in reverse order
      switchd -1;
      return l, r
   end;

   strings.lcs('alexander', 'alex'):

   lcs('alexander', 'alex'): */
static size_t get_common_prefix_len (const char *str, saidx_t idx1, saidx_t idx2, size_t total_len) {
  size_t len = 0;
  while ((idx1 + len < total_len) && (idx2 + len < total_len) && (str[idx1 + len] == str[idx2 + len])) {
    if (str[idx1 + len] == '#') break;
    len++;
  }
  return len;
}

static int str_lcs (lua_State *L) {
  size_t m, n;
  const char *s, *t;
  s = agn_checklstring(L, 1, &m);
  t = agn_checklstring(L, 2, &n);
  if (m == n && memcmp(s, t, m) == 0) {
    luaL_checkstack(L, 2, "not enough stack space");
    lua_pushinteger(L, m);
    lua_pushlstring(L, s, m);
    return 2;
  }
  if (m > (size_t)(-1)/sizeof(saidx_t) || n > (size_t)(-1)/sizeof(saidx_t))  /* 7.9.1 overflow fix */
    luaL_error(L, "Error in " LUA_QS ": input string is too long.", "suffix.lcs");
  size_t prefix_len = 0;
  while (prefix_len < m && prefix_len < n && s[prefix_len] == t[prefix_len]) {
    prefix_len++;
  }
  size_t suffix_len = 0;
  while (m > prefix_len && n > prefix_len && s[m - 1] == t[n - 1]) {
    m--;
    n--;
    suffix_len++;
  }
  size_t max_lcs_len = (prefix_len > suffix_len) ? prefix_len : suffix_len;
  const char *best_match_ptr = (prefix_len > suffix_len) ? s : (s + m);
  const char *s_core = s + prefix_len;
  const char *t_core = t + prefix_len;
  size_t m_core = m - prefix_len;
  size_t n_core = n - prefix_len;
  char *combined = NULL;
  saidx_t *SA = NULL;
  if (m_core > 0 && n_core > 0 && m_core > max_lcs_len && n_core > max_lcs_len) {
    size_t i, tot;
    tot = m_core + 1 + n_core;  /* s_core + '#' + t_core */
    combined = (char *)malloc(tot + 1);
    if (!combined) {
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "strings.lcs");
    }
    memcpy(combined, s_core, m_core);
    combined[m_core] = '#';
    memcpy(combined + m_core + 1, t_core, n_core);
    combined[tot] = '\0';
    SA = (saidx_t *)malloc(tot*sizeof(saidx_t));
    if (!SA) {
      free(combined);
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "strings.lcs");
    }
    if (divsufsort((const unsigned char *)combined, SA, tot) != 0) {
      xfreeall(combined, SA);
      luaL_error(L, "Error in " LUA_QS ": failed to compute suffix array.", "strings.lcs");
    }
    saidx_t best_suffix_idx = 0;
    for (i=0; i < tot - 1; i++) {
      saidx_t idx1 = SA[i];
      saidx_t idx2 = SA[i + 1];
      int is_idx1_in_s1 = (idx1 < (saidx_t)m_core);
      int is_idx2_in_s1 = (idx2 < (saidx_t)m_core);
      if (is_idx1_in_s1 != is_idx2_in_s1) {
        size_t max_room1 = tot - idx1;
        size_t max_room2 = tot - idx2;
        if (max_room1 > max_lcs_len && max_room2 > max_lcs_len) {
          size_t match_len = get_common_prefix_len(combined, idx1, idx2, tot);
          if (match_len > max_lcs_len) {
            max_lcs_len = match_len;
            best_suffix_idx = idx1;
            best_match_ptr = &combined[best_suffix_idx];
          }
        }
      }
    }
  }
  luaL_checkstack(L, 2, "not enough stack space");
  if (max_lcs_len > 0) {
    lua_pushinteger(L, max_lcs_len);
    /*  Safe: Speicher lebt noch und wird von Lua unbeschadet ausgelesen */
    lua_pushlstring(L, best_match_ptr, max_lcs_len);
  } else {
    lua_pushinteger(L, 0);
    lua_pushliteral(L, "");
  }
  if (combined || SA) {
    xfreeall(combined, SA);
  }
  return 2;
}

/* The function checks whether s represents a subsequence of characters of string t.

   A subsequence of a string is a new string which is formed from the original string by deleting some (can be none) of the characters
   without disturbing the relative positions of the remaining characters. (ie, "ace" is a subsequence of "abcde" while "aec" is not).

   Taken from: https://github.com/vli02/leetcode, `My leetcode solutions in C`, Sol. 392, written by Victor vli02. 4.11.0 */

static int str_issubseq (lua_State *L) {
  size_t m, n;
  const char *s, *t;
  s = agn_checklstring(L, 1, &m);
  t = agn_checklstring(L, 2, &n);
  if (m*n == 0) {
    lua_pushfail(L);
    return 1;
  }
  while (*s && *t) {
    while (*t && *t != *s) t++;
    if (*t) { s++; t++; }
  }
  lua_pushboolean(L, !(*s));
  return 1;
}


/* Compares two strings case-insensitively and returns an estimate of their similarity as both an absolute and relative score,
   the latter taking into account the length of the longer string. One point is given for a matching character. Subsequent matches are
   given two extra points. A higher score indicates a higher similarity.
   With the second return, 1 depicts equality, and a lower value the degree of similarity. 2.21.11, C port 4.3.3 */
static int str_fuzzy (lua_State *L) {  /* 4.3.3, 14 times faster than the Agena implementation */
  int i, j, k, nomatch, score, prevmatchpos;
  size_t slen, tlen;
  const char *s, *t;
  char tchar;
  s = agn_checklstring(L, 1, &slen);
  t = agn_checklstring(L, 2, &tlen);
  if (slen*tlen == 0) {  /* at least one of the strings is empty ? */
    lua_pushundefined(L);
    lua_pushundefined(L);
    return 2;
  }
  if (slen > tlen) {
    /* swap strings to consume less memory */
    TOOLS_SWAP(s, t);
    TOOLS_SWAP(slen, tlen);
  }
  i = 0;
  score = 0;
  prevmatchpos = INT_MIN;
  for (j=0; j < tlen; j++) {  /* traverse second (shorter) string */
    tchar = tools_lowercase[uchar(t[j])];
    nomatch = 1;
    for (k=i; k < slen && nomatch; k++) {  /* traverse first (longer) string */
      if (tchar == tools_lowercase[uchar(s[k])]) {  /* match ? */
        score++;  /* simple character results in one point, changed 2.22.0 */
        if (prevmatchpos + 1 == k) score += 2;  /* subsequent matches improve the score */
        prevmatchpos = k;
        nomatch = 0;  /* exit inner loop */
      }
      i = k;
    }
  }
  lua_pushnumber(L, (lua_Number)score);
  lua_pushnumber(L, (lua_Number)score/(lua_Number)((slen - 1)*3 + 1));
  return 2;
}


/* Creates a dynamically allocated copy of a string, changing the encoding from ISO-8859-15 to UTF-8.
   Extended 7.3.6 */
static int str_toutf8 (lua_State *L) {
  size_t c = 0, l;
  unsigned char *t;
  const char *str = agn_checklstring(L, 1, &l);
  int rc = 0, cptoutf8 = agnL_optposint(L, 2, 0);
  if (cptoutf8 == 0) {
    char *r = latin9_to_utf8(str, &l);
    if (!r)
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "strings.toutf8");
    lua_pushlstring(L, (const char*)r, l);
    xfree(r);
    return 1;
  }
  t = (unsigned char *)agn_stralloc(L, 3*l + 1, "strings.toutf8", NULL);
  switch (cptoutf8) {
    case 775: {  /* CP775 (DOS Baltic Rim) */
      c = cp775_to_utf8((unsigned const char *)str, t, &rc);
      break;
    }
    case 850: {  /* CP850 (DOS/OEM Multilingual Latin I) */
      c = cp850_to_utf8((unsigned const char *)str, t, &rc);
      break;
    }
    case 852: {  /* CP852 (DOS Central European) */
      c = cp852_to_utf8((unsigned const char *)str, t, &rc);
      break;
    }
    case 857: {  /* CP857 (DOS Turkish) */
      c = cp857_to_utf8((unsigned const char *)str, t, &rc);
      break;
    }
    case 860: {  /* CP860 (DOS Portuguese) */
      c = cp860_to_utf8((unsigned const char *)str, t, &rc);
      break;
    }
    case 1258: {  /* Windows-1258 (Vietnamese) */
      c = win1258_to_utf8((unsigned const char *)str, t, &rc);
      break;
    }
    default: {
      xfree(t);
      luaL_error(L, "Error in " LUA_QS ": unknown codepage %d, must be 850, 852, 857, 860, 775, 1258.",
        "strings.toutf8", cptoutf8);
    }
  }
  if (rc) lua_pushlstring(L, (const char *)t, c);
  xfree(t);
  if (!rc)
    luaL_error(L, "Error in " LUA_QS ": something went wrong internally.", "strings.toutf8");
  return 1;
}


/* Creates a dynamically allocated copy of a string, changing the encoding from UTF-8 to ISO-8859-1/15.
   Unsupported code points are ignored. Extended 7.3.6 */
static int str_tolatin (lua_State *L) {
  size_t l;
  char *r;
  const char *str = agn_checklstring(L, 1, &l);
  int cptoutf8 = agnL_optposint(L, 2, 0);
  if (cptoutf8 == 0) {
    r = utf8_to_latin9(str, &l);
    if (!r)
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "strings.tolatin");
  } else {
    r = tools_strdup(str);
    if (!r)
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "strings.tolatin");
    switch (cptoutf8) {
      case 775: {  /* CP775 (DOS Baltic Rim) */
        cp775_to_latin4((unsigned char *)r);
        break;
      }
      case 850: {  /* CP850 (DOS/OEM Multilingual Latin I) */
        cp850_to_latin1((unsigned char *)r);
        break;
      }
      case 852: {  /* CP852 (DOS Central European) */
        cp852_to_latin2((unsigned char *)r);
        break;
      }
      case 857: {  /* CP857 (DOS Turkish) */
        cp857_to_latin5((unsigned char *)r);
        break;
      }
      case 860: {  /* CP860 (DOS Portuguese) */
        cp860_to_latin1((unsigned char *)r);
        break;
      }
      default: {
        xfree(r);
        luaL_error(L, "Error in " LUA_QS ": unknown codepage %d, must be 850, 852, 857, 860 or 775.",
          "strings.tolatin", cptoutf8);
      }
    }
  }
  lua_pushlstring(L, (const char *)r, l);
  xfree(r);
  return 1;
}


/* Detects that the given string contains at least one multibyte and return true or false. */
static int str_isutf8 (lua_State *L) {  /* changed 2.16.2 */
  size_t s, l;
  const char *str = agn_checkstring(L, 1);
  s = utf8_to_latin9_len(str, &l);  /* s = Latin length, l = UTF-8 length */
  lua_pushboolean(L, s != l);
  return 1;
}


/* Detects that the given string is in UTF-8 encoding */
static int str_ismultibyte (lua_State *L) {
  int isutf8;
  size_t pos, s, l;
  const char *str;
  str = agn_checkstring(L, 1);
  isutf8 = is_utf8(str, &pos);
  lua_pushboolean(L, isutf8);
  s = utf8_to_latin9_len(str, &l);  /* s = Latin length, l = UTF-8 length */
  lua_pushboolean(L, s != l);
  if (!isutf8) lua_pushinteger(L, pos);  /* 2.12.6 */
  return 2 + (!isutf8);
}


/* Assumes a string is UTF-8 encoded and determines its size */

static int str_utf8size (lua_State *L) {
  lua_pushnumber(L, size_utf8(agn_checkstring(L, 1)));
  return 1;
}


static int _str_glob (lua_State *L) {  /* extended 2.3.4 */
  const char *str0, *pat0;
  char *str, *pat;
  size_t lstr, lpat;
  int insensitive;
  str0 = agn_checklstring(L, 1, &lstr);
  pat0 = agn_checklstring(L, 2, &lpat);
  str = tools_strndup(str0, lstr);  /* 2.39.8 fix */
  pat = tools_strndup(pat0, lpat);  /* diro */
  insensitive = agnL_optboolean(L, 3, 0);
  if (insensitive) {
    size_t i, l1, l2;
    l1 = tools_strlen(str);  /* 2.17.8 tweak */
    l2 = tools_strlen(pat);  /* 2.17.8 tweak */
    for (i=0; i < l1; i++) str[i] = tools_lowercase[uchar(str[i])];
    for (i=0; i < l2; i++) pat[i] = tools_lowercase[uchar(pat[i])];
  }
  agn_pushboolean(L, str_glob((const char*)pat, (const char*)str));
  xfreeall(str, pat);
  return 1;
}


/* str_capitalise: The function is 30% faster than an Agena implementation. */

static int str_capitalise (lua_State *L) {  /* 1.10.0, rewritten 2.37.7, tuned 2.39.12 */
  const char *e, *s, *olds, *sep;
  Charbuf buf;  /* 2.39.12 */
  size_t l, lsep;
  int nargs = lua_gettop(L);
  s = olds = agn_checklstring(L, 1, &l);
  sep = luaL_optlstring(L, 2, " ", &lsep);
  if (l == 0) {
    lua_pushvalue(L, 1);
    return 1;
  }
  if (lsep == 0) { sep = " "; lsep = 1; };
  charbuf_init(L, &buf, 0, l, 1);  /* 2.39.12 */
  /* collect all matching tokens into a sequence */
  if (nargs > 1) {
    while ((e=strstr(s, sep)) != NULL) {
      charbuf_appendchar(L, &buf, tools_uppercase[uchar(*s++)]);
      charbuf_append(L, &buf, s, e - s);
      charbuf_append(L, &buf, sep, lsep);
      s = e + lsep;
    }
  }
  /* process last token, but only if non-empty */
  charbuf_appendchar(L, &buf, tools_uppercase[uchar(*s++)]);
  charbuf_append(L, &buf, s, l - (s - olds));
  charbuf_finish(&buf);  /* better sure than sorry, 2.39.12 */
  lua_pushlstring(L, charbuf_get(&buf), l);
  charbuf_free(&buf);
  return 1;
}


static int str_uncapitalise (lua_State *L) {  /* 1.10.0, based on str_capitalise, rewritten 2.37.7, tuned 2.39.12 */
  const char *e, *s, *olds, *sep;
  Charbuf buf;  /* 2.39.12 */
  size_t l, lsep;
  int nargs = lua_gettop(L);
  s = olds = agn_checklstring(L, 1, &l);
  sep = luaL_optlstring(L, 2, " ", &lsep);
  if (l == 0) {
    lua_pushvalue(L, 1);
    return 1;
  }
  if (lsep == 0) { sep = " "; lsep = 1; };
  charbuf_init(L, &buf, 0, l, 1);  /* 2.39.12 */
  /* collect all matching tokens into a sequence */
  if (nargs > 1) {
    while ((e=strstr(s, sep)) != NULL) {
      charbuf_appendchar(L, &buf, tools_lowercase[uchar(*s++)]);
      charbuf_append(L, &buf, s, e - s);
      charbuf_append(L, &buf, sep, lsep);
      s = e + lsep;
    }
  }
  /* process last token, but only if non-empty */
  charbuf_appendchar(L, &buf, tools_lowercase[uchar(*s++)]);
  charbuf_append(L, &buf, s, l - (s - olds));
  charbuf_finish(&buf);  /* better sure than sorry, 2.39.12 */
  lua_pushlstring(L, charbuf_get(&buf), l);
  charbuf_free(&buf);
  return 1;
}


/* ... If any third argument is passed, then a) the function returns a sequence with one empty string if str is the empty string
  instead of `fail`, and b) if none of the delimiters could be found in str, returns a sequence with str in it instead of `fail`. */
static int str_separate (lua_State *L) {
  size_t c, l_delim, l_string, nofail;
  const char *string, *delim;
  char *result, *str;
  string = agn_checklstring(L, 1, &l_string);
  delim = agn_checklstring(L, 2, &l_delim);
  nofail = lua_gettop(L) > 2;  /* 2.16.2 extension */
  if (l_delim == 0) {
    if (nofail) {  /* got empty string ? -> put one empty string into sequence */
      agn_createseq(L, 1);
      lua_seqrawsetistring(L, -1, 1, "");
    } else
      lua_pushfail(L);
    return 1;
  }
  str = tools_strndup(string, l_string);  /* otherwise strtok would modify the original string at index #1; 2.27.0 tweak */
  result = strtok(str, delim);
  if (result == NULL) {  /* no delimiter found ? */
    if (nofail) {
      agn_createseq(L, 1);
      lua_seqrawsetistring(L, -1, 1, string);  /* put entire (original) string into sequence */
    } else
      lua_pushfail(L);
    goto bailout;
  }
  c = 1;
  agn_createseq(L, 8);
  lua_seqrawsetistring(L, -1, c, result);
  while ((result = strtok(NULL, delim)) != NULL)
    lua_seqrawsetistring(L, -1, ++c, result);
bailout:
  xfree(str);
  return 1;
}


/* strings.diffs: returns the differences between two strings: substitutions, transpositions, deletions,
   and insertions.

   By default, both strings must contains at least three characters. You may change this by passing any other
   positive number as the optional third argument. The function returns `fail` if at least one of the strings
   consists of less characters.

   If any fourth argument is given, the return is a sequence of strings describing the respective difference
   found, otherwise the returns is the number of the differences encountered.

   written July 24, 2007; tuned December 15, 2007; patched January 05, 2008 with correct determination of diffs,
   patched October 10, 2011 for correct results with equal sized strings; extended 2.4.5, March 08, 2015 */

static int str_diffs (lua_State *L) {
  size_t i, j, last, noffsets, option, nstr1, nstr2, c;
  const char *str1 = agn_checklstring(L, 1, &nstr1);
  const char *str2 = agn_checklstring(L, 2, &nstr2);
  int minNumber = luaL_optint(L, 3, 3);
  if (minNumber < 1)
    luaL_error(L, "Error in " LUA_QS ": third argument must be positive.", "strings.diffs");
  option = lua_gettop(L) > 3;
  if (nstr1 < minNumber || nstr2 < minNumber) {
    lua_pushfail(L);
    return 1;
  }
  last = (nstr1 > nstr2) ? nstr2 : nstr1;
  i = j = noffsets = c = 0;
  if (option) agn_createseq(L, 0);
  while (i < last && j < last) {  /* both i and j must be checked ! Invalid subscripts and crashes otherwise */
    if (uchar(str1[i]) != uchar(str2[j])) {
      if (uchar(str1[i + 1]) == uchar(str2[j]) && uchar(str1[i]) == uchar(str2[j + 1])) {
        /* transposition */
        i += 2; j += 2; c++;
        if (option) lua_seqrawsetistring(L, -1, c, "transposition");
        continue;
      }
      else if (uchar(str1[i + 1]) == uchar(str2[j])) {
        /* insertion */
        i += 2; j++; c++;
        if (option) lua_seqrawsetistring(L, -1, c, "insertion");
        noffsets++;
        continue;
      }
      else if (uchar(str1[i]) == uchar(str2[j + 1])) {
        /* deletion */
        i++; j += 2; c++;
        if (option) lua_seqrawsetistring(L, -1, c, "deletion");
        noffsets++;
        continue;
      }
      else if (uchar(str1[i + 1]) == uchar(str2[j + 1])) {
        /* substitution */
        i += 2; j += 2; c++;
        if (option) lua_seqrawsetistring(L, -1, c, "substitution");
        continue;
      } else {  /* should not happen */
        c++;
        if (option) lua_seqrawsetistring(L, -1, c, "other");
      }
    }
    i++; j++;
  }
  if (option && nstr1 != nstr2) {  /* lua_seqrawsetistring is a multiline-macro */
    for (i=0; i < abs(nstr1 - nstr2); i++)
      lua_seqrawsetistring(L, -1, ++c, "deletion");
  } else
    lua_pushnumber(L, c + ((nstr1 - nstr2 == 0) ? 0 : abs(nstr1 - nstr2) - noffsets));  /* October 10, 2011 */
  return 1;
}


/* Table of characters: decimal, hex, character
  65, 41 a  66, 42 b  67, 43 c  68, 44 d  69, 45 e  70, 46 f  71, 47 g
  72, 48 h  73, 49 i  74, 4A j  75, 4B k  76, 4C l  77, 4D m  78, 4E n
  79, 4F o  80, 50 p  81, 51 q  82, 52 r  83, 53 s  84, 54 t  85, 55 u
  86, 56 v  87, 57 w  88, 58 x  89, 59 y  90, 5A z

   Phonetic mapping (not downward-compatible to Soundex since w is regarded a vowel there):

   'a', 'e', 'i', 'o', 'u', 'y', 'h', diacritics -> '#'
   'b', 'p'                                      -> 'P'
   'f', 'v', 'w'                                 -> 'F'
   'd', 't'                                      -> 'T'
   'm', 'n'                                      -> 'N'
   'l'                                           -> 'L'
   'r'                                           -> 'R'
   'j'                                           -> 'J'
   'c', 'k', 'q', 'g'                            -> 'K'
   's', 'z', 'x'                                 -> 'S' */

static unsigned char phQmap[256] = {
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


static int str_phonetiqs (lua_State *L) {  /* October 18, 2008; extended 2.4.5, March 09, 2015 */
  size_t l, i, pos;
  int maxlen, novowels;
  unsigned char c;
  const char *str;
  char *result;
  str = agn_checklstring(L, 1, &l);
  novowels = agnL_optboolean(L, 2, 1);
  maxlen = luaL_optinteger(L, 3, l);
  if (maxlen < 1 || maxlen > l)
    luaL_error(L, "Error in " LUA_QS ": second argument %d is out-of-range.", "strings.phonetiqs", maxlen);
  result = agn_stralloc(L, l, "strings.phonetiqs", NULL);  /* 2.16.5 change */
  for (i=0; i < l; i++)
    result[i] = phQmap[uchar(str[i])];
  result[i] = '\0';
  pos = 0;
  /* this is faster than comparing and inserting a value into the array in only one loop */
  for (i=1; i < l; i++) {
    c = result[i];
    if (((novowels && c != '#') || !novowels) && (c != result[i - 1])) result[++pos] = c;
  }
  result[++pos] = '\0';
  lua_pushlstring(L, result, (pos < maxlen) ? pos : maxlen);
  xfree(result);
  return 1;
}


/* When called with no option, returns the first position where the two strings s1 and s2 differ, or 0 if both strings are equal.
   If called with any option, the function calls the C function strcmp and returns its result:
   "a value that has the same sign as the difference between the first differing pair of characters" (GNU C Library manual) */

/* 100% Locale-safe, UTF-8 safe, and Null-safe comparison */
static int tools_raw_compare (const char *p, size_t lp, const char *l, size_t ll) {
  size_t minl = (lp < ll) ? lp : ll;
  /* memcmp is mandated by C standard to use unsigned char comparison */
  int res = memcmp(p, l, minl);
  if (res == 0) {
    if (lp < ll) return -1;
    if (lp > ll) return 1;
    return 0;
  }
  return res;
}

static int str_compare (lua_State *L) {  /* 2.9.8; 2.25.2 tweak */
  const char *p, *q;
  if (lua_gettop(L) < 3) {
    size_t n, l, l1, l2;
    p = agn_checklstring(L, 1, &l1);  /* 2.25.2 tweak */
    q = agn_checklstring(L, 2, &l2);  /* 2.25.2 tweak */
    l = (l1 < l2) ? l1 : l2;
    n = 0;
    for (; n < l; n++) {
      if (*p++ != *q++) {
        lua_pushinteger(L, n + 1);
        return 1;
      }
    }
    lua_pushinteger(L, (l1 == l2) ? 0 : n + 1);
  } else {  /* now UNDOCUMENTED, due to `strings.strcmp` introduced with 2.25.4 */
    const char *r;
    size_t l_p, l_q;
    p = agn_checklstring(L, 1, &l_p);
    q = agn_checklstring(L, 2, &l_q);
    if (lua_istrue(L, 3)) {
      lua_pushinteger(L, tools_raw_compare(p, l_p, q, l_q));
    } else {
      lua_pushinteger(L, strcmp(p, q));
    }
    r = tools_lmemfind(p, l_p, q, l_q);
    if (r)
      lua_pushstring(L, r);
    else
      lua_pushnil(L);
    return 2;
  }
  return 1;
}


/* Removes the last character from string s and returns the shortened string. If s is empty, it is
   simply returned. Idea taken from the StringUtils.chop procedure of the Apache Commons Lang 3.5 API. */
static int str_chop (lua_State *L) {  /* 2.10.0, based on agenaV_atendof */
  size_t s_len, nargs, istrue;
  const char *s;
  s = agn_checklstring(L, 1, &s_len);
  nargs = lua_gettop(L);
  if (nargs == 0 || nargs > 3)  /* to discern it from strings.chomp */
    luaL_error(L, "Error in " LUA_QS ": only one to three arguments accepted.", "strings.chop");
  luaL_checkstack(L, 2, "not enough stack space");  /* 2.31.6/7 fix */
  if (s_len == 0)
    lua_pushvalue(L, 1);  /* just return empty string */
  else {
    int c, i;
    c = 0;
    if (nargs >= 2 && lua_type(L, 2) == LUA_TFUNCTION) {  /* 2.12.0 RC 2 */
      for (i=s_len - 1; i >= 0; i--) {
        lua_pushvalue(L, 2);
        lua_pushchar(L, s[i]);
        lua_call(L, 1, 1);
        if (!lua_isboolean(L, -1)) {  /* recognises `fail' as a Boolean */
          agn_poptop(L);
          luaL_error(L, "Error in " LUA_QS ": function must evaluate to a Boolean.", "strings.chop");
        }
        istrue = agn_istrue(L, -1);
        agn_poptop(L);
        if (istrue)
          c++;
        else  /* if condition is false, break */
          break;
      }
    } else
      c++;
    lua_pushlstring(L, s, s_len - c);
  }
  return 1;
}


/* Removes pattern string q and additional patterns from the end of the string p if it is there, and returns
   the shortened string p; otherwise returns p unchanged. Idea taken from the StringUtils.chomp procedure of
   the Apache Commons Lang 3.5 API. */
static int str_chomp (lua_State *L) {  /* 2.10.0, based on agenaV_atendof */
  size_t p_len, s_len, olds_len, pmatching, nargs, i;
  const char *s, *p, *olds;
  char *pos;
  nargs = lua_gettop(L);
  if (nargs < 2)  /* 2.28.5 */
    luaL_error(L, "Error in " LUA_QS ": need at least two strings.", "strings.chomp");
  s = agn_checklstring(L, 1, &s_len);
  olds = s;
  olds_len = s_len;
  for (i=2; i <= nargs; i++) {  /* 2.28.5 extension */
    p = agn_checklstring(L, i, &p_len);
    pmatching = tools_hasstrchr(p, SPECIALS, p_len);  /* 2.16.5/2.16.12/2.26.3 optimisation */
    if (p_len == 0 || s_len == 0 || (pmatching == 0 && s_len < p_len) ) {
      /* skip if string or pattern is empty or if pattern is longer or equal in size */
      lua_pushlstring(L, s, s_len);
    } else if (pmatching == 0) {
      s = s + s_len - p_len;
      pos = strstr(s, p);
      if (pos == s)  /* found */
        lua_pushlstring(L, olds, pos - olds);
      else
        lua_pushlstring(L, olds, olds_len);
    } else {  /* 2.12.0 RC 4 */
      int i;
      char pm[p_len + 2];  /* we use a variable-length array (VLA) for match() may throw errors later; 6.1.1 patch */
      pm[p_len] = 0; pm[p_len + 1] = 0;
      for (i=0; i < p_len; i++) pm[i] = p[i];
      if (p[p_len - 1] != '$') { pm[p_len] = '$'; p_len++; }
      s = aux_match(L, s, pm, s_len, 0, &p_len);
      if (p_len == 0)  /* not found */
        lua_pushlstring(L, olds, olds_len);
      else
        lua_pushlstring(L, olds, s_len - p_len);
    }
    olds_len = s_len;
    s = lua_tolstring(L, -1, &s_len);
    olds = s;
    agn_poptop(L);
  }
  lua_pushlstring(L, s, s_len);
  return 1;
}


/* Appends suffix q (a string) to p (a string) if q is not already at the end of p; otherwise returns p.
   If p is the empty string, q is returned. Idea taken from the StringUtils.appendIfMissing procedure of
   the Apache Commons Lang 3.5 API. */
static int str_appendmissing (lua_State *L) {  /* 2.10.0, based on agenaV_atendof */
  size_t p_len, s_len;
  const char *s, *p;
  char *pos;
  s = agn_checklstring(L, 1, &s_len);
  p = agn_checklstring(L, 2, &p_len);
  if (s_len == 0)
    lua_pushvalue(L, 2);
  else if (p_len == 0 || s_len < p_len)
    /* bail out if string or pattern is empty or if pattern is longer or equal in size */
    lua_pushvalue(L, 1);
  else {
    s = s + s_len - p_len;
    pos = strstr(s, p);
    if (pos == s)  /* match */
      lua_pushvalue(L, 1);
    else {
      lua_pushvalue(L, 1);
      lua_pushvalue(L, 2);
      lua_concat(L, 2);
    }
  }
  return 1;
}


/* Wraps a string s with another strings t, returning the Agena equivalent of t & s & t.

   If the third argument is `true`, only wraps if t is missing at the start and the end of s; otherwise
   simply returns s.

   Idea taken from the StringUtils.wrapIfMissing procedure of the Apache Commons Lang 3.5 API. */
static int str_wrap (lua_State *L) {  /* 2.10.0; based on str_iswrapped, extended 2.39.1 */
  size_t s_len, p_len;
  const char *s, *p;
  char *pos;
  s = agnL_tolstringx(L, 1, &s_len, "strings.wrap");  /* 2.39.1 improvement, taken from former skycrane.enclose */
  p = agn_checklstring(L, 2, &p_len);
  if (p_len == 0)
    lua_pushvalue(L, 1);
  else if (agnL_optboolean(L, 3, 0)) {
    pos = strstr(s, p);
    if (pos == s)  /* match at the start */
      lua_pushvalue(L, 1);
    else {
      const char *s0 = s + s_len - p_len;
      pos = strstr(s0, p);
      if (pos != s0) {
        lua_pushvalue(L, 2);
        lua_pushlstring(L, s, s_len);
        lua_pushvalue(L, 2);
        lua_concat(L, 3);
      } else
        lua_pushvalue(L, 1);
    }
  } else {
    lua_pushvalue(L, 2);
    lua_pushstring(L, s);
    lua_pushvalue(L, 2);
    lua_concat(L, 3);
  }
  return 1;
}


/* Removes an enclosing character chr from string str and returns the modified string i.e. with chr deleted from both
   the start and end of str. One or more potential enclosing characters are given in delim, which defaults to `"'`.

   With the first enclosing character in delim found in str, the function returns the shortened string.

   If str is not enclosed by any of the characters in delim, str will be returned unmodified. 2.39.1 */
static int str_unwrap (lua_State *L) {
  size_t l1, l2, i;
  const char *str, *delim;
  str = agn_checklstring(L, 1, &l1);
  delim = luaL_optlstring(L, 2, "\'\"", &l2);  /* empty strings will be ignored */
  for (i=0; l1 > 1 && i < l2; i++) {
    if (str[0] == delim[i] && str[l1 - 1] == delim[i]) {
      lua_pushlstring(L, ++str, l1 - 2);
      return 1;
    }
  }
  lua_pushvalue(L, 1);
  return 1;
}


/* Checks whether the string s is wrapped by string p and returns true or false. If p is the
   empty string, the function returns `fail`. */
static int str_iswrapped (lua_State *L) {  /* 2.10.0 */
  size_t p_len, s_len;
  const char *s, *p;
  char *pos;
  s = agn_checklstring(L, 1, &s_len);
  p = agn_checklstring(L, 2, &p_len);
  if (p_len == 0)
    lua_pushfail(L);
  else if (s_len < p_len)
    lua_pushfalse(L);
  else {
    pos = strstr(s, p);
    if (pos != s)  /* no match at the start */
      lua_pushfalse(L);
    else {
      s = s + s_len - p_len;
      pos = strstr(s, p);
      lua_pushboolean(L, pos == s);
    }
  }
  return 1;
}


/* Returns the substring in string s that is nested between the prefix string p and the suffix string q.
   Idea taken from the StringUtils.substringBetween procedure of the Apache Commons Lang 3.5 API. */
static int str_between (lua_State *L) {  /* 2.10.0; based on str_iswrapped, extended 2.12.0 RC 4 */
  size_t s_len, p_len, q_len, r_len, tonumber;
  const char *s, *p, *q;
  char *r;
  s = agn_checklstring(L, 1, &s_len);
  p = agn_checklstring(L, 2, &p_len);
  q = agn_checklstring(L, 3, &q_len);
  if (p_len < 1 || q_len < 1)  /* 2.11.4 RC 4 */
    luaL_error(L, "Error in " LUA_QS ": patterns must be non-empty.", "strings.between");
  tonumber = agnL_optboolean(L, 4, 0);
  r = tools_between(s, s_len, p, p_len, q, q_len, &r_len);  /* 2.27.4 */
  if (!r || r_len == s_len)
    lua_pushnil(L);
  else {
    lua_pushlstring(L, r, r_len);
    if (tonumber) agnL_strtonumber(L, -1);  /* 2.12.0 RC 4 */
  }
  return 1;
}


/* Checks whether all characters in string s are part of the characters in string p and returns `true`
   or `false`. Idea taken from the StringUtils.containsAny procedure of the Apache Commons Lang 3.5 API.
   This implementation is much faster than using strspn (strspn(s, p) == strlen(s)). See also strings.has. */
static int str_contains (lua_State *L) {  /* 2.10.0 */
  const char *s, *p;
  size_t sl, sp;
  s = agn_checklstring(L, 1, &sl);
  p = agn_checklstring(L, 2, &sp);
  if (sp == 0)
    luaL_error(L, "Error in " LUA_QS ": second argument is the empty string.", "strings.contains");
  lua_pushboolean(L, tools_strinalphabet(s, p, sl, sp));  /* 2.39.3, 20 % faster */
  return 1;
}


/* The function takes a string s to be split apart into its tokens one after another, and a delimiter
   string d, and returns an iterator function that each time it is called, returns one token. If the end
   of s has been reached, the function returns `null`. The function supports pattern matching.

   If the string starts with the delimiter, an empty string is returned.

   If called with any argument, the function returns the number of tokens returned but does not search
   for the next token. 2.10.0

   See also: split, strings.fields, strings.separate. */

static int gseparate_iterator (lua_State *L) {
  const char *s, *s2, *p, *unwrap;
  size_t l1, l2, counter;
  int pmatching, tonumber, unwraplen;
  ptrdiff_t init;
  s = agn_tostring(L, lua_upvalueindex(1));
  p = agn_tostring(L, lua_upvalueindex(2));
  l1 = agn_tonumber(L, lua_upvalueindex(3));
  l2 = agn_tonumber(L, lua_upvalueindex(4));
  init = agn_tonumber(L, lua_upvalueindex(5));
  counter = agn_tonumber(L, lua_upvalueindex(6));
  pmatching = agn_tonumber(L, lua_upvalueindex(7));    /* 2.12.0 RC 4 */
  tonumber = agn_tonumber(L, lua_upvalueindex(8));     /* 2.12.0 RC 4 */
  unwrap = lua_tostring(L, lua_upvalueindex(9));       /* 2.39.1 */
  unwraplen = lua_tointeger(L, lua_upvalueindex(10));  /* 2.39.1 */
  if (lua_gettop(L) > 0) {  /* just query counter, but do not search for next token */
    lua_pushinteger(L, counter);
    return 1;
  }
  if (init >= l1) {
    lua_pushnil(L);
  } else {
    /* We do not use strtok with this iterator as the source string cannot be reliently duplicated and freed.
       strtok modifies the source string arbitrarily. */
    s2 = (pmatching == 0) ? tools_lmemfind(s + init, l1 - init, p, l2) :   /* 2.16.12 tweak */
                            aux_match(L, s + init, p, l1 - init, 0, &l2);  /* 4.10.8 patch */
    if (s2 == NULL) {
      lua_pushstring(L, s + init);
      init = l1 + l2;  /* 4.10.8 patch */
    } else {
      lua_pushlstring(L, s + init, s2 - s - init);
      init = s2 - s + l2;  /* 4.10.8 patch */
    }
    lua_pushinteger(L, init);
    lua_replace(L, lua_upvalueindex(5));  /* update cursor */
    lua_pushinteger(L, ++counter);
    lua_replace(L, lua_upvalueindex(6));  /* update counter */
  }
  if (unwrap) agnL_strunwrap(L, -1, unwrap, unwraplen);
  if (tonumber) agnL_strtonumber(L, -1);  /* 2.12.0 RC 4 */
  return 1;  /* return token */
}

static int str_gseparate (lua_State *L) {
  const char *s, *p;
  size_t l1, l2;
  int nargs, init, tonumber, match, bailout, header, ignorefnidx, skipfaultylines, hasspecials;
  char *unwrap, *sep;
  nargs = lua_gettop(L);
  tonumber = header = ignorefnidx = skipfaultylines = 0;
  init = 1;
  sep = unwrap = NULL;
  s = agn_checklstring(L, 1, &l1);
  p = agn_checklstring(L, 2, &l2);
  if (l2 == 0)
    luaL_error(L, "Error in " LUA_QS ": second argument is the empty string.", "strings.gseparate");
  if (!agnL_isvalidpattern(L, p, l2, &hasspecials, 1)) {  /* 6.1.1 fix, invalid pattern given */
    luaL_error(L, "Error in " LUA_QS ": %s", "strings.gseparate", agn_tostring(L, -1));
  }
  aux_splitcheckoptions(L, 0, &nargs, 2, &init, &tonumber, &match, &bailout, &unwrap, &sep,
    &header, &ignorefnidx, &skipfaultylines, "strings.gseparate");  /* 2.39.1 */
  if (nargs > 2 && lua_isboolean(L, 3))
    tonumber = lua_toboolean(L, 3);
  init = (nargs > 3 && agn_isinteger(L, 4)) ?
    tools_posrelat(agn_tointeger(L, 4), l1) - 1 : tools_posrelat(init, l1) - 1;
  if (init < 0) init = 0;
  else if (init > l1) init = l1;  /* iterator returns null right from the start */
  luaL_checkstack(L, 10, "not enough stack space");  /* 2.39.1 fix */
  lua_pushstring(L, s);
  lua_pushstring(L, p);
  lua_pushinteger(L, l1);    /* push lengths */
  lua_pushinteger(L, l2);
  lua_pushinteger(L, init);  /* push init cursor */
  lua_pushinteger(L, 0);     /* push counter */
  lua_pushinteger(L, tools_hasstrchr(p, SPECIALS, l2));  /* use pattern matching, 2.16.5/2.16.12/2.26.3 optimisation */
  lua_pushinteger(L, tonumber);
  lua_pushstring(L, unwrap);  /* pushes null (not 'null') if unwrap == NULL */
  lua_pushinteger(L, tools_strlen(unwrap));  /* if unwrap is NULL, tools_strlen returns 0 both on 32-bit and 64-bit platforms */
  lua_pushcclosure(L, &gseparate_iterator, 10);
  xfreeall(sep, unwrap);
  return 1;
}

/*
f := strings.gseparate('123;456;789;0', ';');
f():

g := strings.gseparate('abc;def;ghi;z', ';');
g():
*/

/* The function takes a string s to be split into two pieces, and a string d of one or more single-character
   delimiters, and returns two values: the first part of s up to - but not including - the delimiter found,
   and the rest of s also without the delimiter.

   If a string cannot be split apart, it is returned as the first result and the second return is `null`.

   See also: split, strings.gseparate, strings.separate. */
static int str_cut (lua_State *L) {  /* 2.10.0 */
  const char *s, *p;
  char *r;
  s = agn_checkstring(L, 1);
  p = agn_checkstring(L, 2);
  r = strpbrk(s, p);  /* we need the exact matching position, so do not use tools_strpbrk */
  if (r == NULL) {    /* delimiter not found ? */
    lua_pushstring(L, s);
    lua_pushnil(L);
  } else {
    lua_pushlstring(L, s, r - s);
    if (*(++r) != '\0')
      lua_pushstring(L, r);
    else
      lua_pushnil(L);
  }
  return 2;
}


/* The function moves to substring p in s and returns p up to the end of s. If p could not be
   found, the function returns `null`. The function supports captures.

   If a positive integer pos is given as the second argument, the substring starting at position pos (an integer) up to the end of s is returned.
   If pos is greater than the length of s, the result is null.

   If the optional third argument `true` is given, the function returns the rest of s following
   but not including p. In this case, if s ends with p, `null` is returned.

   See also: strings.find. */
static int str_advance (lua_State *L) {
  size_t l1, l2;
  const char *s1, *s, *p;
  int movepastp;
  s = agn_checklstring(L, 1, &l1);
  if (agn_isinteger(L, 2)) {  /* new 2.27.2 */
    ptrdiff_t offset = tools_posrelat(lua_tointeger(L, 2), l1) - 1;
    if (offset > l1 - 1)
      lua_pushnil(L);
    else if (offset == 0)
      lua_pushvalue(L, 1);
    else
      lua_pushlstring(L, s + offset, l1 - offset);
    return 1;
  }
  p = agn_checklstring(L, 2, &l2);
  movepastp = agnL_optboolean(L, 3, 0);
  if (l2 == 0) {  /* else garbage */
    lua_pushnil(L);
    return 1;
  }
  /* 2.12.0 RC 4 extension, 2.16.5/2.16.12/2.26.3 optimisation */
  s1 = (tools_hasstrchr(p, SPECIALS, l2)) ? aux_match(L, s, p, l1, 0, &l2) :
                                            tools_lmemfind(s, l1, p, l2);
  if (!s1) {  /* bail out */
    lua_pushnil(L);
    return 1;
  }
  if (!movepastp)
    lua_pushstring(L, s1);
  else {
    if (*(s1 + l2) != '\0')
      lua_pushstring(L, s1 + l2);
    else
      lua_pushnil(L);
  }
  return 1;
}


/* Shannon entropy, taken from: https://rosettacode.org/wiki/Entropy#C */
size_t shannon_makehist (const char *s, int *hist, int len) {
  int wherechar[256];
  unsigned char c;  /* patched 2.26.2 */
  size_t i, histlen;
  histlen = 0;
  for (i=0; i < 256; i++) wherechar[i] = -1;
  for (i=0; i < len; i++) {
    c = uchar(s[i]);  /* patched 2.26.2 */
    if (wherechar[c] == -1) {
      wherechar[c] = histlen;
      histlen++;
    }
    hist[wherechar[c]]++;  /* hist stores number of occurrences per unique (wherechar) char */
  }
  return histlen;  /* number of unique characters */
}

double shannon_entropy (int *hist, int histlen, int len) {
  size_t i;
  double H;
  H = 0;
  for (i=0; i < histlen; i++) {
    H -= (double)hist[i]/len*sun_log2((double)hist[i]/len);  /* 2.14.13, changed to sun_log */
  }
  return H;
}

/* Returns the normalised specific Shannon entropy, the specific Shannon entropy, and the total information entropy (in bits), in this order.
   The function does not look for any patterns that might be available for compression, so its use is quite limited and `gzip.deflate` might be
   a better alternative. 2.12.0 RC 1 */
static int str_shannon (lua_State *L) {
  size_t l;
  int *hist, histlen;
  const char *s;
  lua_Number H;
  s = agn_checklstring(L, 1, &l);
  luaL_checkstack(L, 3, "not enough stack space");  /* 4.7.1 fix */
  if (l == 0) {
    lua_pushundefined(L); lua_pushundefined(L); lua_pushundefined(L);
    return 3;
  }
  hist = (int*)calloc(l, sizeof(int));
  if (hist == NULL)  /* 7.3.4 fix */
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "strings.shannon");
  histlen = shannon_makehist(s, hist, l);
  /* hist now has no order (known to the programme) but that doesn't matter */
  H = shannon_entropy(hist, histlen, l);
  xfree(hist);
  lua_pushnumber(L, H*LN2/log(l));  /* normalised specific entropy */
  lua_pushnumber(L, H);    /* specific entropy */
  lua_pushnumber(L, H*l);  /* total information entropy in bits */
  return 3;
}


/* Returns the entropy of the given string str as developed by John Walker, Fourmilab. By default str is considered to be a
   byte stream, otherwise, if the second argument is set to true, to be a bitstream. If the third argument is `true`, then the
   function also returns the serial correlation coefficient, the Chi square distribution, the mean of the ASCII values in str,
   and the Monte Carlo value for Pi. See also `strings.shannon`. 2.37.0 */
static int str_walker (lua_State *L) {
  int getall, binary;
  const char *str;
  unsigned char ocb;
  size_t l;
  long int totalc;
  double montepi, scc, ent, mean, chisq;
  binary = agnL_optboolean(L, 2, 0);  /* treat as binary (1), as bitstream (0) */
  getall = agnL_optboolean(L, 3, 0);
  str = agn_checklstring(L, 1, &l);
  /* initialise for calculations */
  rt_init(binary);
  /* scan input and count character occurrences */
  while (l--) {
    ocb = uchar(*str++);
    rt_add(&ocb, 1);  /* add character */
  }
  /* complete calculation and return sequence metrics */
  rt_end(&ent, &chisq, &mean, &montepi, &scc, &totalc);
  lua_pushnumber(L, ent);        /* entropy */
  if (getall) {
    lua_pushnumber(L, scc);      /* serial correlation coefficient */
    lua_pushnumber(L, chisq);    /* Chi square distribution */
    lua_pushnumber(L, mean);
    lua_pushnumber(L, montepi);  /* Monte Carlo value for Pi */
  }
  return 1 + 4*getall;
}


/* Returns a set of all the unique characters included in a string, 2.12.0 RC 1 */
static int str_charset (lua_State *L) {
  const char *s;
  size_t l, i;
  unsigned char wherechar[256];
  s = agn_checklstring(L, 1, &l);
  agn_createset(L, 8);
  for (i=0; i < 256; i++) wherechar[i] = 0;
  while (*s) {
    if (wherechar[uchar(*s)] == 0) {
      wherechar[uchar(*s)] = 1;
      lua_srawsetlstring(L, -1, s, 1);
    }
    s++;
  }
  return 1;
}


/* Taken from musl-1.2.2/src/misc/a64l.c, MIT licence, https://www.musl-libc.org/download.html
   The function converts between 32-bit long integers and little-endian base-64 ASCII strings (of length 0 to 6).
   If the argument is a base-64 ASCII string, the result is a signed 32-bit integer; if the argument is a number, the result is
   the base-64 ASCII string. */
static int32_t aux_a64l (const char *s) {
  int e;
  uint32_t x = 0;
  for (e=0; e < 36 && *s; e += 6, s++) {
    const char *d = strchr(tools_b64, *s);
    if (!d) break;  /* invalid char ? */
    x |= (uint32_t)(d - tools_b64) << e;
  }
  return (int32_t)x;
}

static char *aux_l64a (uint32_t x0) {
  static char s[7];
  char *p;
  uint32_t x = x0;
  for (p=s; x; p++, x >>= 6)
    *p = tools_b64[x & 63];
  *p = 0;
  return s;
}

static int str_a64 (lua_State *L) {  /* 2.12.6 */
  if (agn_isstring(L, 1))
    lua_pushinteger(L, aux_a64l(agn_tostring(L, 1)));
  else if (agn_isnumber(L, 1)) {
    lua_Number x = agn_tonumber(L, 1);
    if (tools_isfrac(x) || x < LUAI_MININT32 || x > LUAI_MAXINT32)
      luaL_error(L, "Error in " LUA_QS ": number not an integer or out of range.", "strings.a64");
    lua_pushstring(L, aux_l64a(x));
  } else
    luaL_error(L, "Error in " LUA_QS ": expected either a string or number, got %s.", "strings.a64", luaL_typename(L, 1));
  return 1;
}


static int str_itoa (lua_State *L) {  /* 2.12.7, keep undocumented */
  char *s = tools_itoa(agn_checkinteger(L, 1), agnL_optposint(L, 2, 10));
  if (s)
    lua_pushstring(L, s);
  else
    lua_pushfail(L);
  return 1;
}


/* Creates a random string of the given fixed `length`. By default, a Base64 string consisting of the characters
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" is returned. If the second argument is
   "ascii", a random ASCII string consisting of characters in the range ASCI 32 to ASCII 127 is returned. You
   can change the upper and lower ASCII bounds by explicitly passing the non-negative integers l and u. */
static int str_random (lua_State *L) {  /* 2.14.3 */
  int nops, i, l, u;
  size_t len;
  const char *mode;
  unsigned char *r;
  nops = agn_checknonnegint(L, 1);
  mode = luaL_optlstring(L, 2, "base64", &len);
  if (len == 0)
    luaL_error(L, "Error in " LUA_QS ": second argument must not be the empty string.", "strings.random");
  if (tools_streq(mode, "base64")) {
    l = agnL_optnonnegint(L, 3, 0) % 64;
    u = agnL_optnonnegint(L, 4, 63) % 64;
    if (u < l) {
      luaL_error(L, "Error in " LUA_QS ": upper bound %d is less than lower bound %d.", "strings.random", u, l);
    }
    r = (unsigned char *)agn_stralloc(L, nops, "strings.random", NULL);  /* 7.3.5 change */
    /* for (i=0; i < nops; i++) {
      r[i] = tools_cb64[tools_randomrange(l, u, i == 0)]; } */ /* 4.0.2 change: set seeds only once */
    str_randname((char *)r, nops, tools_cb64, 64, l, u);  /* 5.6.2, 15 times faster */
  } else if (tools_streq(mode, "ascii")) {  /* 'ascii' must be explicitly given since 2.38.0, as documented before */
    l = agnL_optnonnegint(L, 3, 32) % 256;
    u = agnL_optnonnegint(L, 4, 126) % 256;  /* 2.16.1 change, DEL key = ASCII code 127 excluded */
    if (u < l) {
      luaL_error(L, "Error in " LUA_QS ": upper bound %d is less than lower bound %d.", "strings.random", u, l);
    }
    r = (unsigned char *)agn_stralloc(L, nops, "strings.random", NULL);  /* 7.3.5 change */
    for (i=0; i < nops; i++) {
      /* r[i] = (unsigned char)tools_randomrange(l, u, i == 0); */ /* 4.0.2 change: set seeds only once */
      r[i] = (unsigned char)tools_randint(l, u, i == 0);  /* 5.6.2 change */
    }
  } else {  /* character set has been given, 2.38.0 */
    int reallyrandom = agnL_optboolean(L, 3, 1);  /* 1= create a really random string, 0 = when called in sequential
      order, the `random` strings will always be the same. 3.9.4 */
    r = (unsigned char *)agn_stralloc(L, nops, "strings.random", NULL);  /* 7.3.5 change */
    l = 0;
    u = len - 1;
    for (i=0; i < nops; i++) {
      /* r[i] = mode[tools_randomrange(l, u, reallyrandom && i == 0)]; */ /* 4.0.2 change: set seeds (if requested) only once */
      r[i] = mode[tools_randint(l, u, reallyrandom && i == 0)];  /* 5.6.2 change */
    }
  }
  lua_pushlstring(L, (const char *)r, nops);
  xfree(r);
  return 1;
}


/* Returns the length of string s: the first return is the result of the call to the internal C function strlen,
   and the second return is the internally stored length of s, returned by Agena's `size` operator.

   The difference between strlen and `size` is that C's strlen only counts the number of characters up to and excluding the
   first embedded zero (i.e. character '\0'), whereas `size` returns the real length including embedded zeros, but without
   the terminating zero.

   Example:

   > s := 'abc' & char(0) & 'defgh';  # 3 chars up to the first embedded zero, 9 chars at all

   > strings.strlen(s):
   3       9 */
static int str_strlen (lua_State *L) {  /* 2.16.1 */
  size_t l, chunks;
  int nargs = lua_gettop(L);  /* 2.39.11 fix */
  lua_pushnumber(L, tools_strlen(agn_checklstring(L, 1, &l)));  /* 2.17.8 tweak; changed from standard strlen to tools_strlen in 2.25.1 */
  lua_pushnumber(L, l);
  if (nargs > 1) {  /* 2.25.5 UNDOC */
    lua_pushnumber(L, tools_optstrlen(l, &chunks));  /* optimal length in bytes and ... */
    lua_pushnumber(L, chunks);                       /* ... corresponding 4/8-byte word chunks */
  }
  return 2 + 2*(nargs > 1);
}


/* Taken from: https://github.com/ashvin-bhuttoo/CryptoTest, written by Ashvin Bhuttoo; no licence information &
   available as of September 26, 2019
   plus: https://stackoverflow.com/questions/41109005/bitwise-circular-shifting-in-an-array, written by John Bupit

   Only this mix of functions taken from the two sources works. */
#define SIZEUCHAR (8*sizeof(unsigned char))

int rshift_mask[8] = { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F };

int lshift_mask[8] = { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };

void shift_left (char* buf, int len, int shift) {
  int i;
  char byte, byte2;
  uint8_t circular_carry;
  circular_carry = (buf[0] & lshift_mask[shift]) >> (SIZEUCHAR - shift);
  byte = 0; byte2 = 0;
  for (i=len - 1; i >= 0; i--) {
    if (i == len - 1) {
      byte = buf[i];
      buf[i] <<= shift;
    } else {
      byte2 = buf[i];
      buf[i] <<= shift;
      buf[i] |= ((byte & lshift_mask[shift]) >> (SIZEUCHAR - shift));
      byte = byte2;
    }
  }
  buf[len - 1] += circular_carry;
  /* do not shift: buf[len - 1] >>= shift; */
  buf[len] = '\0';
}

void shift_right (char *buf, int len, int shift) {
  int i;
  char circular_carry = (buf[len - 1] & rshift_mask[shift]) << (SIZEUCHAR - shift);
  for (i=len - 1; i > 0; i--) {
    buf[i] >>= shift;  /* right shift the (i)th byte */
    buf[i] |= (buf[i - 1] & rshift_mask[shift]) << (SIZEUCHAR - shift);  /* carry from the previous digit */
  }
  buf[0] >>= shift;
  buf[0] += circular_carry;
}

int xorit (char *buf, int len, int xorkey, char variance) {
  int i;
  for (i=0; i < len; i++) {
    buf[i] = (buf[i]) ^ xorkey ^ (i*(variance ? 2 : 0));
    if (buf[i] == 0) return 1;  /* NULL inserted into buf ? -> bail out */
  }
  return 0;
}

/* Rotates all the bits in the string str n bits to the right, with n in range 0 .. 7.

   The n bits dropping off the end of the string will be prepended to the resulting string, so that there is no information
   loss when calling strings.rotateleft to decrypt it.

   You can optionally xor the string by passing the third argument xorkey, an integer in the range 0 .. 255. By setting
   the optional fourth argument `xorval` to true, you can achieve further obfuscation of the string while xoring. */
static int str_rotateright (lua_State *L) {  /* 2.16.3 */
  char *buf, xorvar;
  const char *s;
  int xorkey;
  size_t l, shift;
  s = agn_checklstring(L, 1, &l);
  shift = agn_checknonnegint(L, 2);     /* in bits */
  if (shift > 7)
    luaL_error(L, "Error in " LUA_QS ": can only shift by at most seven bits, got %d.", "strings.rotateright", shift);
  xorkey = agnL_optnonnegint(L, 3, 0);  /* key for xoring, in range 0 .. 255, e.g. 0x3C */
  xorvar = agnL_optboolean(L, 4, 0);    /* variate while xoring ? */
  buf = agn_stralloc(L, l, "strings.rotateright", NULL);  /* 2.16.5 change, 7.3.5 fix */
  tools_memcpy(buf, s, l);  /* 2.21.5 tweak; 2.39.10 fix: don't add 1 to l */
  shift_right(buf, l, shift);
  if (xorkey != 0) {
    if (xorit(buf, l, xorkey, xorvar)) {
      xfree(buf);
      luaL_error(L, "Error in " LUA_QS ": xoring scrambled string, try another key.", "strings.rotateright", shift);
    }
  }
  lua_pushstring(L, (char *)buf);
  xfree(buf);
  return 1;
}


/* Rotates all the bits in the string str n bits to the left, with n in range 0 .. 7.

   The n bits dropping off the beginning of the string will be appended to the resulting string, so that there is no
   information loss when calling strings.rotateright to decrypt it.

   For optional arguments `xorkey` and `xorval` see `strings.rotateright`. */
static int str_rotateleft (lua_State *L) {  /* 2.16.3 */
  char *buf, xorvar;
  const char *s;
  int xorkey;
  size_t l, shift;
  s = agn_checklstring(L, 1, &l);
  shift = agn_checknonnegint(L, 2);     /* in bits */
  if (shift > 7)
    luaL_error(L, "Error in " LUA_QS ": can only shift by at most seven bits, got %d.", "strings.rotateleft", shift);
  xorkey = agnL_optnonnegint(L, 3, 0);  /* in range 0 .. 255, e.g. 0x3C */
  xorvar = agnL_optboolean(L, 4, 0);    /* variate while xoring ? */
  buf = agn_stralloc(L, l, "strings.rotateleft", NULL);  /* 2.16.5 change, 7.3.5 fix */
  tools_memcpy(buf, s, l);  /* 2.21.5 tweak; 2.39.10 fix: don't add 1 to l */
  if (xorkey != 0) {
    if (xorit(buf, l, xorkey, xorvar)) {
      xfree(buf);
      luaL_error(L, "Error in " LUA_QS ": xoring scrambled string, try another key.", "strings.rotateleft", shift);
    }
  }
  shift_left(buf, l, shift);
  lua_pushstring(L, (char *)buf);
  xfree(buf);
  return 1;
}


/* The function checks whether a string is aligned on a 4-byte word boundary and returns true or false. */
static int str_isaligned (lua_State *L) {  /* 2.16.12 */
  const char *s;
  s = agn_checkstring(L, 1);
  lua_pushboolean(L, tools_strisaligned(s));
  return 1;
}


static int chariterate (lua_State *L) {  /* 2.25.2 */
  const char *str;
  size_t n, flag;
  ptrdiff_t pos, strsize;
  str = lua_tostring(L, lua_upvalueindex(1));
  pos = lua_tonumber(L, lua_upvalueindex(2));
  n = lua_tointeger(L, lua_upvalueindex(3));
  flag = lua_tointeger(L, lua_upvalueindex(4));
  strsize = lua_tointeger(L, lua_upvalueindex(5));
  if (flag && pos <= strsize && pos + n - 1 > strsize) {  /* shorten last chunk */
    n = strsize - pos + 1;
    lua_pushinteger(L, 0);
    lua_replace(L, lua_upvalueindex(4));
  }
  if (pos + n - 1 > strsize) {
    lua_pushnil(L);
  } else {
    lua_pushnumber(L, pos + n);
    lua_replace(L, lua_upvalueindex(2));
    lua_pushlstring(L, str + pos - 1, n);
  }
  return 1;
}

static int fourbyteiterate (lua_State *L) {  /* 2.25.2 */
  const char *str;
#ifdef IS32BIT
  uint32_t p;
#else
  uint32_t p, q;
#endif
  int rc, tobigendian;
  size_t l;
  str = lua_tolstring(L, lua_upvalueindex(1), &l);
  tobigendian = lua_tointeger(L, lua_upvalueindex(3));
#ifdef IS32BIT
  /* rc = 0: last chunk, rc = 1: more to come, rc = -1: invalid call */
  p = tools_strtouint32(str, l, &rc, tobigendian);
  if (rc == -3) {  /* 7.7.7 fix */
    luaL_error(L, "Error: memory allocation failed.");
  }
  if (rc == -1 || lua_tointeger(L, lua_upvalueindex(2)) == 0) {  /* we are at the end of the string */
    lua_pushnil(L);
    return 1;
  }
  if (rc == 1) {  /* end of string not yet reached (got full 4-byte chunk) ? */
    str += AGN_BLOCKSIZE;
    lua_pushstring(L, str);
    lua_replace(L, lua_upvalueindex(1));
  } else if (rc == 0) {  /* rc == 0: last few chars */
    lua_pushinteger(L, 0);  /* stop flag */
    lua_replace(L, lua_upvalueindex(2));
  }
  lua_pushinteger(L, p);  /* 2.27.2 change */
#else
  if (lua_tointeger(L, lua_upvalueindex(2)) == 0) {  /* we are at the end of the string; 2.27.3 fix */
    lua_pushnil(L);
    return 1;
  }
  if (lua_tointeger(L, lua_upvalueindex(4))) {  /* we have hx from the previous call in the cache, 2.27.2 */
    lua_pushinteger(L, 0);  /* reset `isincache' flag */
    lua_replace(L, lua_upvalueindex(4));
    lua_pushinteger(L, lua_tointeger(L, lua_upvalueindex(5)));  /* push hx */
    lua_pushinteger(L, 0);  /* and empty cache */
    lua_replace(L, lua_upvalueindex(5));
    if (lua_tointeger(L, lua_upvalueindex(2)) != 0) {  /* stop flag not set ? */
      if (l > AGN_BLOCKSIZE) {  /* we have something to read next ? Else prevent invalid reads; 2.27.3 fix */
        str += AGN_BLOCKSIZE;
        lua_pushstring(L, str);
        lua_replace(L, lua_upvalueindex(1));
      } else {
        lua_pushinteger(L, 0);  /* set stop flag */
        lua_replace(L, lua_upvalueindex(2));
      }
    }
    return 1;
  }
  q = tools_strtouint32(str, l, &rc, &p, tobigendian);
  /* rc = -1: do not push second number, 0: push second number, 1: nothing to push */
  if (rc == 1) {  /* we are at the end of the string */
    lua_pushnil(L);
    return 1;
  }
  lua_pushinteger(L, p);  /* 2.27.2 change */
  /* we will put q in the cache, 2.27.2 change */
  if (rc == 0) {
    lua_pushinteger(L, 1);
    lua_replace(L, lua_upvalueindex(4));
    lua_pushinteger(L, q);
    lua_replace(L, lua_upvalueindex(5));
  } else {  /* last few chars */
    lua_pushinteger(L, 0);  /* set stop flag */
    lua_replace(L, lua_upvalueindex(2));
  }
#endif
  return 1;
}

static int brkiterate (lua_State *L) {  /* 2.25.4 */
  int tonumber, unwraplen;
  const char *str, *delim, *r, *unwrap;
  str = lua_tostring(L, lua_upvalueindex(1));
  delim = lua_tostring(L, lua_upvalueindex(2));
  unwrap = lua_tostring(L, lua_upvalueindex(3));  /* 2.39.1 */
  unwraplen = lua_tointeger(L, lua_upvalueindex(4));  /* 2.39.1 */
  tonumber = lua_tointeger(L, lua_upvalueindex(5));  /* 2.39.1 */
  if (!str) {
    lua_pushnil(L);
    return 1;
  }
  r = strpbrk(str, delim);
  if (r == NULL) {  /* last field */
    lua_pushstring(L, str);
    if (unwrap) agnL_strunwrap(L, -1, unwrap, unwraplen);
    if (tonumber) agnL_strtonumber(L, -1);
    lua_pushstring(L, NULL);  /* pushes null, not 'null' */
  } else {
    lua_pushlstring(L, str, r - str);
    if (unwrap) agnL_strunwrap(L, -1, unwrap, unwraplen);
    if (tonumber) agnL_strtonumber(L, -1);
    lua_pushstring(L, ++r);
  }
  lua_replace(L, lua_upvalueindex(1));
  return 1;
}

/* In the first form, returns an iterator function that, when called returns the next n characters stored in string str, starting at position pos. pos and n are 1 by default.
   In the second form, when pos is 0, returns an iterator function that from the left to right returns each four consecutive characters in string str as an unsigned 4-byte integer.
   In the third form, by passing a string of one or more delimiters as the second argument, returns an iterator function that step-by-step returns a field surrounded
   by at least one of the delimiters.

   If there are no more characters to process, the factory returns `null`. See also: strings.tobytes; 2.25.2 */
static int str_iterate (lua_State *L) {
  ptrdiff_t pos;
  size_t chunks, l;
  int init, nargs, tonumber, match, bailout, header, ignorefnidx, skipfaultylines;
  char *unwrap, *sep;
  nargs = lua_gettop(L);
  tonumber = header = ignorefnidx = skipfaultylines = 0;
  sep = unwrap = NULL;
  agn_checklstring(L, 1, &l);
  aux_splitcheckoptions(L, 0, &nargs, 2, &init, &tonumber, &match, &bailout, &unwrap, &sep,
    &header, &ignorefnidx, &skipfaultylines, "strings.iterate");  /* 2.39.1 */
  luaL_checkstack(L, 5, "not enough stack space");  /* 2.39.1 fix */
  if (agn_isstring(L, 2) || sep) {  /* return fields separated by a delimiter, 2.25.4 */
    size_t l_del;
    int sepfree = (sep != NULL);
    int unwrapfree = (unwrap != NULL);
    if (!sep)
      sep = (char *)lua_tolstring(L, 2, &l_del);
    else
      l_del = tools_strlen(sep);
    if (l_del == 0) {
      if (sepfree) { xfree(sep); }
      if (unwrapfree) { xfree(unwrap); }
      luaL_error(L, "Error in " LUA_QS ": second string must be non-empty.", "strings.iterate");
    }
    if (!unwrap && nargs > 2)
      unwrap = (char *)agnL_optstring(L, 3, NULL);  /* 2.39.1 */
    lua_pushvalue(L, 1);        /* the string */
    lua_pushstring(L, sep);     /* the delimiter */
    lua_pushstring(L, unwrap);  /* the enclosing wrapper to be removed */
    lua_pushinteger(L, tools_strlen(unwrap));  /* size of wrapper char */
    lua_pushinteger(L, tonumber);  /* try to convert to number */
    lua_pushcclosure(L, &brkiterate, 5);
    if (sepfree) xfree(sep);
    if (unwrapfree) xfree(unwrap);
    return 1;
  }
  xfreeall(unwrap, sep);
  pos = (ptrdiff_t)tools_posrelat(agnL_optnonnegint(L, 2, 1), l);
  if (pos < 0) {
    luaL_error(L, "Error in " LUA_QS ": start index must be positive.", "strings.iterate");
  }
  luaL_checkstack(L, 5, "not enough stack space");  /* 3.18.4 fix */
  if (pos == 0) {  /* return uint32_t's */
    int tobigendian = agnL_optboolean(L, 3, 0);  /* check option here before pushing values ! */
    lua_pushvalue(L, 1);        /* the string */
    lua_pushinteger(L, 1);      /* not-last chunk flag */
    lua_pushinteger(L, tobigendian);  /* Endianness, default: don't convert to Big Endian, 2.27.2 */
    lua_pushinteger(L, 0);      /* is there a result in the cache ?, 2.27.2 */
    lua_pushinteger(L, 0);      /* cache for hx, 2.27.2 */
    lua_pushcclosure(L, &fourbyteiterate, 5);
  } else {  /* return characters */
    chunks = (size_t)agnL_optposint(L, 3, 1);  /* check option here before pushing values ! */
    lua_pushvalue(L, 1);        /* the string */
    lua_pushnumber(L, pos);     /* start position */
    lua_pushnumber(L, chunks);  /* number of consecutive characters per call */
    lua_pushinteger(L, 1);      /* last chunk flag */
    lua_pushinteger(L, l);      /* string length */
    lua_pushcclosure(L, &chariterate, 5);
  }
  return 1;
}


/* The function compares two version strings. It is a direct interface to the GNU C strverscmp function.
   The following is a summary of the GNU documentation:

   "If you have files jan1, jan2, ..., jan9, jan10, ..., it feels wrong when an application orders them
   jan1, jan10, ..., jan2, ..., jan9, because the expected order is just: jan1, jan2, ..., jan9, jan10, ...

   The function returns an integer less than, equal to, or greater than zero if s1 is found, respectively,
   to be earlier than, equal to, or later than s2.

   Both input strings should be in plain ASCII." 2.25.4 */
static int str_strverscmp (lua_State *L) {
  if (lua_isnil(L, 1) || lua_isnil(L, 2)) {  /* 5.0.3 extension: undefined behaviour */
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, tools_strverscmp(agn_checkstring(L, 1), agn_checkstring(L, 2)));
  }
  return 1;
}


static int str_strcmp (lua_State *L) {  /* 2.25.4 */
  if (lua_isnil(L, 1) || lua_isnil(L, 2)) {  /* 5.0.3 extension: undefined behaviour */
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, tools_strcmp(agn_checkstring(L, 1), agn_checkstring(L, 2)));
  }
  return 1;
}


static int str_stricmp (lua_State *L) {  /* 2.38.4 */
  if (lua_isnil(L, 1) || lua_isnil(L, 2)) {  /* 5.0.3 extension: undefined behaviour */
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, tools_stricmp(agn_checkstring(L, 1), agn_checkstring(L, 2)));
  }
  return 1;
}


static int str_strncmp (lua_State *L) {  /* 2.38.4 */
  if (lua_isnil(L, 1) || lua_isnil(L, 2)) {  /* 5.0.3 extension: undefined behaviour */
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, tools_strncmp(agn_checkstring(L, 1), agn_checkstring(L, 2), agn_checknonnegint(L, 3)));
  }
  return 1;
}


static int str_strcoll (lua_State *L) {  /* 2.37.8 */
  const char *oldlocale = NULL;  /* to prevent compiler warnings */
  int option = lua_gettop(L) == 3;  /* 3.10.1 extension */
  if (option) {
    const char *s = agn_checkstring(L, 3);
    oldlocale = setlocale(LC_COLLATE, NULL);
    setlocale(LC_COLLATE, s);
  }
  lua_pushinteger(L, strcoll(agn_checkstring(L, 1), agn_checkstring(L, 2)));
  if (option) {
    setlocale(LC_COLLATE, oldlocale);
  }
  return 1;
}


static int str_strspn (lua_State *L) {  /* 2.37.8 */
  if (lua_isnil(L, 1) || lua_isnil(L, 2)) {  /* 5.0.3 extension: undefined behaviour */
    lua_pushnil(L);
  } else {
    const char *s, *p;
    size_t sl, pl;
    s = agn_checklstring(L, 1, &sl);
    p = agn_checklstring(L, 2, &pl);
    /* https://aticleworld.com/strspn-in-c/: "The behavior is undefined if either s1 or s2 is not a pointer to a null-terminated byte string." */
    lua_pushinteger(L, sl*pl == 0 ? 0 : strspn(s, p));
  }
  return 1;
}


static int str_strcspn (lua_State *L) {  /* 2.37.8 */
  if (lua_isnil(L, 1) || lua_isnil(L, 2)) {  /* 5.0.3 extension: undefined behaviour */
    lua_pushnil(L);
  } else {
    const char *s, *p;
    size_t sl, pl;
    s = agn_checklstring(L, 1, &sl);
    p = agn_checklstring(L, 2, &pl);
    /* https://aticleworld.com/strspn-in-c/: "The behavior is undefined if either s1 or s2 is not a pointer to a null-terminated byte string." */
    lua_pushinteger(L, sl*pl == 0 ? 0 : strcspn(s, p));
  }
  return 1;
}


/* The function is an interface to the C strstr function, searches haystack for a substring needle and returns a
   substring starting from the first match to the end of haystack. The second return is the position of the match, starting from 1.
   It returns null and 0 if no match was found. If needle is an empty string, the function returns haystack. */
#define retresult(L,r,s) { \
  luaL_checkstack(L, 2, "not enough stack space"); \
  if (r == NULL) { \
    lua_pushnil(L); \
    lua_pushinteger(L, 0); \
  } else { \
    lua_pushstring(L, r); \
    lua_pushinteger(L, r - s + 1); \
  } \
}

static int str_strstr (lua_State *L) {  /* 2.25.4 */
  if (lua_isnil(L, 1) || lua_isnil(L, 2)) {  /* 5.0.3 extension: undefined behaviour */
    luaL_checkstack(L, 2, "not enough stack space");
    lua_pushnil(L);
    lua_pushnil(L);
  } else {
    const char *s, *r;
    s = agn_checkstring(L, 1);
    r = strstr(s, agn_checkstring(L, 2));
    retresult(L, r, s);  /* checks for 2 stack slots automatically */
  }
  return 2;
}


static int str_strnstr (lua_State *L) {  /* 2.37.7 */
  if (lua_isnil(L, 1) || lua_isnil(L, 2)) {  /* 5.0.3 extension: undefined behaviour */
    luaL_checkstack(L, 2, "not enough stack space");
    lua_pushnil(L);
    lua_pushnil(L);
  } else {
    const char *s, *p, *r;
    size_t l, slen;
    s = agn_checklstring(L, 1, &l);
    p = agn_checkstring(L, 2);
    slen = agnL_optnonnegint(L, 3, l);
    if (slen > l)
      luaL_error(L, "Error in " LUA_QS ": given length %d > string length %d, got %d.", "strings.strnstr", slen, l);
    r = tools_strnstr(s, p, slen);
    retresult(L, r, s);
  }
  return 2;
}


/* The function is an interface to the C strchr function, searches haystack for a single character represented by its
   ASCII code and returns a substring starting from the first match to the end of haystack. The second return is the position
   of the match, starting from 1. It returns null and 0 if no match was found and issues an error if needle is non-positive.
   If needle is an empty string, the function returns haystack. */
static int str_strchr (lua_State *L) {  /* 2.25.4 */
  if (lua_isnil(L, 1)) {  /* 5.0.3 extension: undefined behaviour */
    luaL_checkstack(L, 2, "not enough stack space");
    lua_pushnil(L);
    lua_pushnil(L);
  } else {
    const char *s, *r;
    unsigned char c;
    s = agn_checkstring(L, 1);
    if (agn_isnumber(L, 2))
      c = agn_checkposint(L, 2) % 256;
    else if (agn_isstring(L, 2)) { /* 2.37.0 extension */
      const char *p = lua_tostring(L, 2);
      if (tools_strlen(p) > 1)
        luaL_error(L, "Error in " LUA_QS ": second argument must be one single character.", "strings.strchr");  /* 5.0.3 fix */
      c = uchar(p[0]);
    } else {
      c = 0;
      luaL_error(L, "Error in " LUA_QS ": expected ASCII code or single character for 2nd argument, got %s.", "strings.strchr",
        luaL_typename(L, 2));
    }
    r = strchr(s, c);
    retresult(L, r, s);
  }
  return 2;
}


/* The function is an interface to the C strchr function, searches backwards from the end of haystack for a single character
   represented by its ASCII code and returns a substring starting from the first match to the end of haystack. The second return is
   the position of the match, starting from 1.  It returns null if no match was found and issues an error if needle is non-positive.
   If needle is an empty string, the function returns haystack. */
static int str_strrchr (lua_State *L) {  /* 2.25.4 */
  if (lua_isnil(L, 1)) {  /* 5.0.3 extension: undefined behaviour */
    luaL_checkstack(L, 2, "not enough stack space");
    lua_pushnil(L);
    lua_pushnil(L);
  } else {
    const char *s, *r;
    unsigned char c;
    s = agn_checkstring(L, 1);
    if (agn_isnumber(L, 2))  /* 5.0.3 extension */
      c = agn_checkposint(L, 2) % 256;
    else if (agn_isstring(L, 2)) {
      const char *p = lua_tostring(L, 2);
      if (tools_strlen(p) > 1)
        luaL_error(L, "Error in " LUA_QS ": second argument must be one single character.", "strings.strchr");
      c = uchar(p[0]);
    } else {
      c = 0;
      luaL_error(L, "Error in " LUA_QS ": expected ASCII code or single character for 2nd argument, got %s.", "strings.strrchr",
        luaL_typename(L, 2));
    }
    r = strrchr(s, c);
    retresult(L, r, s);
  }
  return 2;
}


/*
** {===================================================================
** PACK/UNPACK taken from Lua 5.4.6, PUC Rio, MIT licence, Agena 3.10.1
** No changes with respect to Lua 5.4.7 and Lua 5.4.8.
** ====================================================================
*/

/*
** translate a relative initial string position
** (negative means back from end): clip result to [1, inf).
** The length of any string in Lua must fit in a lua_Integer,
** so there are no overflows in the casts.
** The inverted comparison avoids a possible overflow
** computing '-pos'.
*/
static size_t posrelatI (lua_Integer pos, size_t len) {
  if (pos > 0)
    return (size_t)pos;
  else if (pos == 0)
    return 1;
  else if (pos < -(lua_Integer)len)  /* inverted comparison */
    return 1;  /* clip to 1 */
  else return len + (size_t)pos + 1;
}

/* value used for padding */
#if !defined(LUAL_PACKPADBYTE)
#define LUAL_PACKPADBYTE		0x00
#endif

/* maximum size for the binary representation of an integer */
#define MAXINTSIZE	16
/* number of bits in a character */
#define NB	CHAR_BIT
/* mask for one character (NB 1's) */
#define MC	((1 << NB) - 1)
/* size of a lua_Integer */
#define SZINT	((int)sizeof(lua_Integer))

/* dummy union to get native endianness */
static const union {
  int dummy;
  char little;  /* true iff machine is little endian */
} nativeendian = {1};

/*
** information to pack/unpack stuff
*/
typedef struct Header {
  lua_State *L;
  int islittle;
  int maxalign;
} Header;

/*
** options for pack/unpack
*/
typedef enum KOption {
  Kint,		/* signed integers */
  Kuint,	/* unsigned integers */
  Kfloat,	/* single-precision floating-point numbers */
  Knumber,	/* Lua "native" floating-point numbers */
  Kdouble,	/* double-precision floating-point numbers */
  Kchar,	/* fixed-length strings */
  Kstring,	/* strings with prefixed length */
  Kzstr,	/* zero-terminated strings */
  Kpadding,	/* padding */
  Kpaddalign,	/* padding for alignment */
  Knop		/* no-op (configuration or spaces) */
} KOption;

/*
** Read an integer numeral from string 'fmt' or return 'df' if
** there is no numeral
*/
static int digit (int c) { return '0' <= c && c <= '9'; }

static int getnum (const char **fmt, int df) {
  if (!digit(**fmt))  /* no number? */
    return df;  /* return default value */
  else {
    int a = 0;
    do {
      a = a*10 + (*((*fmt)++) - '0');
    } while (digit(**fmt) && a <= ((int)MAXSIZE - 9)/10);
    return a;
  }
}

/*
** Read an integer numeral and raises an error if it is larger
** than the maximum size for integers.
*/
static int getnumlimit (Header *h, const char **fmt, int df) {
  int sz = getnum(fmt, df);
  if (l_unlikely(sz > MAXINTSIZE || sz <= 0))
    return luaL_error(h->L, "integral size (%d) out of limits [1,%d]",
                            sz, MAXINTSIZE);
  return sz;
}

/*
** Initialize Header
*/
static void initheader (lua_State *L, Header *h) {
  h->L = L;
  h->islittle = nativeendian.little;
  h->maxalign = 1;
}

/*
** Read and classify next option. 'size' is filled with option's size.
*/
static KOption getoption (Header *h, const char **fmt, int *size) {
  /* dummy structure to get native alignment requirements */
  struct cD { char c; union { LUAI_MAXALIGN; } u; };
  int opt = *((*fmt)++);
  *size = 0;  /* default */
  switch (opt) {
    case 'b': *size = CHARSIZE; return Kint;
    case 'B': *size = CHARSIZE; return Kuint;
    case 'h': *size = sizeof(short); return Kint;
    case 'H': *size = sizeof(short); return Kuint;
    case 'l': *size = sizeof(long); return Kint;
    case 'L': *size = sizeof(long); return Kuint;
    case 'j': *size = sizeof(lua_Integer); return Kint;
    case 'J': *size = sizeof(lua_Integer); return Kuint;
    case 'T': *size = sizeof(size_t); return Kuint;
    case 'f': *size = sizeof(float); return Kfloat;
    case 'n': *size = sizeof(lua_Number); return Knumber;
    case 'd': *size = sizeof(double); return Kdouble;
    case 'i': *size = getnumlimit(h, fmt, sizeof(int)); return Kint;
    case 'I': *size = getnumlimit(h, fmt, sizeof(int)); return Kuint;
    case 's': *size = getnumlimit(h, fmt, sizeof(size_t)); return Kstring;
    case 'c':
      *size = getnum(fmt, -1);
      if (l_unlikely(*size == -1))
        luaL_error(h->L, "missing size for format option 'c'");
      return Kchar;
    case 'z': return Kzstr;
    case 'x': *size = 1; return Kpadding;
    case 'X': return Kpaddalign;
    case ' ': break;
    case '<': h->islittle = 1; break;
    case '>': h->islittle = 0; break;
    case '=': h->islittle = nativeendian.little; break;
    case '!': {
      const int maxalign = offsetof(struct cD, u);
      h->maxalign = getnumlimit(h, fmt, maxalign);
      break;
    }
    default: luaL_error(h->L, "invalid format option '%c'", opt);
  }
  return Knop;
}

/*
** Read, classify, and fill other details about the next option.
** 'psize' is filled with option's size, 'notoalign' with its
** alignment requirements.
** Local variable 'size' gets the size to be aligned. (Kpadal option
** always gets its full alignment, other options are limited by
** the maximum alignment ('maxalign'). Kchar option needs no alignment
** despite its size.
*/
static KOption getdetails (Header *h, size_t totalsize,
                           const char **fmt, int *psize, int *ntoalign) {
  KOption opt = getoption(h, fmt, psize);
  int align = *psize;  /* usually, alignment follows size */
  if (opt == Kpaddalign) {  /* 'X' gets alignment from following option */
    if (**fmt == '\0' || getoption(h, fmt, &align) == Kchar || align == 0)
      luaL_argerror(h->L, 1, "invalid next option for option 'X'");
  }
  if (align <= 1 || opt == Kchar)  /* need no alignment? */
    *ntoalign = 0;
  else {
    if (align > h->maxalign)  /* enforce maximum alignment */
      align = h->maxalign;
    if (l_unlikely((align & (align - 1)) != 0))  /* not a power of 2? */
      luaL_argerror(h->L, 1, "format asks for alignment not power of 2");
    *ntoalign = (align - (int)(totalsize & (align - 1))) & (align - 1);
  }
  return opt;
}

/*
** Pack integer 'n' with 'size' bytes and 'islittle' endianness.
** The final 'if' handles the case when 'size' is larger than
** the size of a Lua integer, correcting the extra sign-extension
** bytes if necessary (by default they would be zeros).
*/
static void packint (luaL_Buffer *b, lua_Unsigned n,
                     int islittle, int size, int neg) {
  char *buff = luaL_prepbuffsize(b, size);
  int i;
  buff[islittle ? 0 : size - 1] = (char)(n & MC);  /* first byte */
  for (i = 1; i < size; i++) {
    n >>= NB;
    buff[islittle ? i : size - 1 - i] = (char)(n & MC);
  }
  if (neg && size > SZINT) {  /* negative number need sign extension? */
    for (i = SZINT; i < size; i++)  /* correct extra bytes */
      buff[islittle ? i : size - 1 - i] = (char)MC;
  }
  luaL_addsize(b, size);  /* add result to buffer */
}

/*
** Copy 'size' bytes from 'src' to 'dest', correcting endianness if
** given 'islittle' is different from native endianness.
*/
static void copywithendian (char *dest, const char *src,
                            int size, int islittle) {
  if (islittle == nativeendian.little)
    memcpy(dest, src, size);
  else {
    dest += size - 1;
    while (size-- != 0)
      *(dest--) = *(src++);
  }
}

static int str_pack (lua_State *L) {
  luaL_Buffer b;
  Header h;
  const char *fmt = luaL_checkstring(L, 1);  /* format string */
  int arg = 1;  /* current argument to pack */
  size_t totalsize = 0;  /* accumulate total size of result */
  initheader(L, &h);
  lua_pushnil(L);  /* mark to separate arguments from string buffer */
  luaL_buffinit(L, &b);
  while (*fmt != '\0') {
    int size, ntoalign;
    KOption opt = getdetails(&h, totalsize, &fmt, &size, &ntoalign);
    totalsize += ntoalign + size;
    while (ntoalign-- > 0)
     luaL_addchar(&b, LUAL_PACKPADBYTE);  /* fill alignment */
    arg++;
    switch (opt) {
      case Kint: {  /* signed integers */
        lua_Integer n = luaL_checkinteger(L, arg);
        if (size < SZINT) {  /* need overflow check? */
          lua_Integer lim = (lua_Integer)1 << ((size*NB) - 1);
          luaL_argcheck(L, -lim <= n && n < lim, arg, "integer overflow");
        }
        packint(&b, (lua_Unsigned)n, h.islittle, size, (n < 0));
        break;
      }
      case Kuint: {  /* unsigned integers */
        lua_Integer n = luaL_checkinteger(L, arg);
        if (size < SZINT)  /* need overflow check? */
          luaL_argcheck(L, (lua_Unsigned)n < ((lua_Unsigned)1 << (size*NB)),
                           arg, "unsigned overflow");
        packint(&b, (lua_Unsigned)n, h.islittle, size, 0);
        break;
      }
      case Kfloat: {  /* C float */
        float f = (float)luaL_checknumber(L, arg);  /* get argument */
        char *buff = luaL_prepbuffsize(&b, sizeof(f));
        /* move 'f' to final result, correcting endianness if needed */
        copywithendian(buff, (char *)&f, sizeof(f), h.islittle);
        luaL_addsize(&b, size);
        break;
      }
      case Knumber: {  /* Lua float */
        lua_Number f = luaL_checknumber(L, arg);  /* get argument */
        char *buff = luaL_prepbuffsize(&b, sizeof(f));
        /* move 'f' to final result, correcting endianness if needed */
        copywithendian(buff, (char *)&f, sizeof(f), h.islittle);
        luaL_addsize(&b, size);
        break;
      }
      case Kdouble: {  /* C double */
        double f = (double)luaL_checknumber(L, arg);  /* get argument */
        char *buff = luaL_prepbuffsize(&b, sizeof(f));
        /* move 'f' to final result, correcting endianness if needed */
        copywithendian(buff, (char *)&f, sizeof(f), h.islittle);
        luaL_addsize(&b, size);
        break;
      }
      case Kchar: {  /* fixed-size string */
        size_t len;
        const char *s = luaL_checklstring(L, arg, &len);
        luaL_argcheck(L, len <= (size_t)size, arg,
                         "string longer than given size");
        luaL_addlstring(&b, s, len);  /* add string */
        while (len++ < (size_t)size)  /* pad extra space */
          luaL_addchar(&b, LUAL_PACKPADBYTE);
        break;
      }
      case Kstring: {  /* strings with length count */
        size_t len;
        const char *s = luaL_checklstring(L, arg, &len);
        luaL_argcheck(L, size >= (int)sizeof(size_t) ||
                         len < ((size_t)1 << (size*NB)),
                         arg, "string length does not fit in given size");
        packint(&b, (lua_Unsigned)len, h.islittle, size, 0);  /* pack length */
        luaL_addlstring(&b, s, len);
        totalsize += len;
        break;
      }
      case Kzstr: {  /* zero-terminated string */
        size_t len;
        const char *s = luaL_checklstring(L, arg, &len);
        luaL_argcheck(L, strlen(s) == len, arg, "string contains zeros");
        luaL_addlstring(&b, s, len);
        luaL_addchar(&b, '\0');  /* add zero at the end */
        totalsize += len + 1;
        break;
      }
      case Kpadding: luaL_addchar(&b, LUAL_PACKPADBYTE);  /* FALLTHROUGH */
      case Kpaddalign: case Knop:
        arg--;  /* undo increment */
        break;
    }
  }
  luaL_pushresult(&b);
  return 1;
}


static int str_packsize (lua_State *L) {
  Header h;
  const char *fmt = luaL_checkstring(L, 1);  /* format string */
  size_t totalsize = 0;  /* accumulate total size of result */
  initheader(L, &h);
  while (*fmt != '\0') {
    int size, ntoalign;
    KOption opt = getdetails(&h, totalsize, &fmt, &size, &ntoalign);
    luaL_argcheck(L, opt != Kstring && opt != Kzstr, 1,
                     "variable-length format");
    size += ntoalign;  /* total space used by option */
    luaL_argcheck(L, totalsize <= MAXSIZE - size, 1,
                     "format result too large");
    totalsize += size;
  }
  lua_pushinteger(L, (lua_Integer)totalsize);
  return 1;
}


/*
** Unpack an integer with 'size' bytes and 'islittle' endianness.
** If size is smaller than the size of a Lua integer and integer
** is signed, must do sign extension (propagating the sign to the
** higher bits); if size is larger than the size of a Lua integer,
** it must check the unread bytes to see whether they do not cause an
** overflow.
*/
static lua_Integer unpackint (lua_State *L, const char *str,
                              int islittle, int size, int issigned) {
  lua_Unsigned res = 0;
  int i;
  int limit = (size  <= SZINT) ? size : SZINT;
  for (i = limit - 1; i >= 0; i--) {
    res <<= NB;
    res |= (lua_Unsigned)(unsigned char)str[islittle ? i : size - 1 - i];
  }
  if (size < SZINT) {  /* real size smaller than lua_Integer? */
    if (issigned) {  /* needs sign extension? */
      lua_Unsigned mask = (lua_Unsigned)1 << (size*NB - 1);
      res = ((res ^ mask) - mask);  /* do sign extension */
    }
  }
  else if (size > SZINT) {  /* must check unread bytes */
    int mask = (!issigned || (lua_Integer)res >= 0) ? 0 : MC;
    for (i = limit; i < size; i++) {
      if (l_unlikely((unsigned char)str[islittle ? i : size - 1 - i] != mask))
        luaL_error(L, "%d-byte integer does not fit into Lua Integer", size);
    }
  }
  return (lua_Integer)res;
}


static int str_unpack (lua_State *L) {
  Header h;
  const char *fmt = luaL_checkstring(L, 1);
  size_t ld;
  const char *data = luaL_checklstring(L, 2, &ld);
  size_t pos = posrelatI(luaL_optinteger(L, 3, 1), ld) - 1;
  int n = 0;  /* number of results */
  luaL_argcheck(L, pos <= ld, 3, "initial position out of string");
  initheader(L, &h);
  while (*fmt != '\0') {
    int size, ntoalign;
    KOption opt = getdetails(&h, pos, &fmt, &size, &ntoalign);
    luaL_argcheck(L, (size_t)ntoalign + size <= ld - pos, 2,
                    "data string too short");
    pos += ntoalign;  /* skip alignment */
    /* stack space for item + next position */
    luaL_checkstack(L, 2, "too many results");
    n++;
    switch (opt) {
      case Kint:
      case Kuint: {
        lua_Integer res = unpackint(L, data + pos, h.islittle, size,
                                       (opt == Kint));
        lua_pushinteger(L, res);
        break;
      }
      case Kfloat: {
        float f;
        copywithendian((char *)&f, data + pos, sizeof(f), h.islittle);
        lua_pushnumber(L, (lua_Number)f);
        break;
      }
      case Knumber: {
        lua_Number f;
        copywithendian((char *)&f, data + pos, sizeof(f), h.islittle);
        lua_pushnumber(L, f);
        break;
      }
      case Kdouble: {
        double f;
        copywithendian((char *)&f, data + pos, sizeof(f), h.islittle);
        lua_pushnumber(L, (lua_Number)f);
        break;
      }
      case Kchar: {
        lua_pushlstring(L, data + pos, size);
        break;
      }
      case Kstring: {
        size_t len = (size_t)unpackint(L, data + pos, h.islittle, size, 0);
        luaL_argcheck(L, len <= ld - pos - size, 2, "data string too short");
        lua_pushlstring(L, data + pos + size, len);
        pos += len;  /* skip string */
        break;
      }
      case Kzstr: {
        size_t len = strlen(data + pos);
        luaL_argcheck(L, pos + len < ld, 2,
                         "unfinished string for format 'z'");
        lua_pushlstring(L, data + pos, len);
        pos += len + 1;  /* skip string plus final '\0' */
        break;
      }
      case Kpaddalign: case Kpadding: case Knop:
        n--;  /* undo increment */
        break;
    }
    pos += size;
  }
  lua_pushinteger(L, pos + 1);  /* next position */
  return n + 1;
}

/* End of Lua pack/packsize/unpack */


static int str_join (lua_State *L) {  /* 2.34.5 */
  int nargs = lua_gettop(L);
  if (nargs < 1) {
    luaL_error(L, "Error in " LUA_QS ": need at least one argument, got %d.", "strings.join", nargs);
  }
  agn_join(L, 1, nargs);
  return 1;
}


static int str_replace (lua_State *L) {  /* 2.34.5 */
  int nargs = lua_gettop(L);
  if (nargs < 2) {
    luaL_error(L, "Error in " LUA_QS ": need at least two arguments, got %d.", "strings.replace", nargs);
  }
  agn_replace(L, 1, nargs);
  return 1;
}


static int str_instr (lua_State *L) {  /* 2.34.5 */
  int nargs = lua_gettop(L);
  if (nargs < 2) {
    luaL_error(L, "Error in " LUA_QS ": need at least two arguments, got %d.", "strings.instr", nargs);
  }
  agn_instr(L, 1, nargs);
  return 1;
}


static int str_lower (lua_State *L) {  /* 4.3.3 */
  agn_lower(L, 1, lua_gettop(L));
  return 1;
}


static int str_upper (lua_State *L) {  /* 4.3.3 */
  agn_upper(L, 1, lua_gettop(L));
  return 1;
}


static int str_trim (lua_State *L) {  /* 4.3.3 */
  agn_trim(L, 1, lua_gettop(L));
  return 1;
}


/* Tries to convert a non-negative integer of base base represented by string s to a (decimal) value and if successful
   returns it or pushes `undefined` otherwise. If the third argument is `true`, then the decimal value and the empty
   string, or zero and the rest of s where parsing failed, are returned. The function provides an interface to the underlying
   strtoul C function. 2.39.3 */
static int str_strtoul (lua_State *L) {
  int base, full, success, en;
  size_t l;
  char *s, *endptr;
  lua_Number r, sign;
  if (lua_isnil(L, 1)) {  /* 5.0.3 extension for undefined behaviour */
    lua_pushnil(L);
    return 1;
  }
  s = (char *)agn_checklstring(L, 1, &l);
  if (l == 0)
    luaL_error(L, "Error in " LUA_QS ": first argument is the empty string.", "strings.strtoul");
  base = agn_checkposint(L, 2);
  full = agnL_optboolean(L, 3, 0) == 1;
  sign = 1;
  if (l > 1 && s[0] == '-') {  /* at least in MinGW, negative values always result in 4294967281, so autocorrect */
    s++; sign = -1;
  } else if (l > 1 && s[0] == '+') {
    s++;  /* since strtoul may not cope with plus signs, we just skip it */
  }
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  r = strtoull(s, &endptr, base);  /* 2.39.4 extension */
  en = errno;  /* 2.39.4 fix for borderline cases */
  success = (en != ERANGE && (endptr == NULL || endptr[0] == '\0'));  /* we have to check for both results */
  if (full) {
    lua_pushnumber(L, sign*r);
    lua_pushstring(L, success ? "" : endptr);
  } else {
    lua_pushnumber(L, success ? sign*r : AGN_NAN);  /* ULONG_MAX = 4294967295 = 2^32-1 indicates an error */
  }
  return 1 + full;
}


/* Transforms the multibyte string `s` using the collation transformation determined by the locale currently
   selected for collation, and returns the transformed string. The function is a wrapper to the C function
   strxfrm() but spares the effort to compute the exact size of the output. See also: `os.setlocale`. 5.4.3 */
static int str_strxfrm (lua_State *L) {
  size_t l;
  char *to = NULL;
  const char *from = agn_checkstring(L, 1);
  l = strxfrm(to, from, 0);  /* get exact size of the resulting string */
  to = tools_stralloc(l);
  if (!to)  /* 7.3.5 fix */
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "strings.strxfrm");
  l = strxfrm(to, from, l);
  lua_pushlstring(L, to, l);
  xfree(to);
  return 1;
}


/* Takes the number of seconds elapsed since the epoch or a table of date and time components and returns a formatted timestamp, a string.

   In the first form, the `seconds' argument represents the number of seconds elapsed since a given epoch (usually January 01, 1970).

   In the second form, the function receives a date and optionally time of the form year, month, date [, hour [, minute [, second [, milliseconds]]]],
   with all values put into the table, sequence or register obj in this order, and a format string.

   Copy the date and time specifiers table of os.date.  %Q specifier !  no 'z'

   Example:

   > secs := os.datetosecs([2010, 1, 1, 12, 30, 1, 123]):
   1262345401

   > strings.strftime(secs, '%Y-%m-%dT%H:%M:%SZ'):
   2010-01-01T12:30:01Z

   > strings.strftime([2010, 1, 1, 12, 30, 1, 123], '%Y-%m-%dT%H:%M:%S.%QZ'):
   2010-01-01T12:30:01.123Z

   5.5.7

   DJGPP has serious accuracy problems with localtime() and gmtime(), at least when run in a virtual machine.
*/
static int _str_strftime (lua_State *L) {
  struct TM *stm;
  int ms = 0;
  const char *fmt;
  Time64_T t;
/* #if defined(__DJGPP__)
  luaL_error(L, "Error in " LUA_QS ": the function cannot be called in DOS.", "strings.strftime");
#endif */
  if (agn_isnonnegint(L, 1)) {
    t = lua_tointeger(L, 1);
  } else if (lua_istable(L, 1) || lua_isseq(L, 1) || lua_isreg(L, 1)) {
    t = agnL_datetosecs(L, 1, "strings.strftime", 1, 0, &ms);
    if (t < 0) luaL_error(L, "Error in " LUA_QS ": could not determine date or date out-of-range.", "strings.strftime");
  } else {
    luaL_error(L, "Error in " LUA_QS ": argument #1 must be a number, table, sequence or register.", "strings.strftime");
  }
  fmt = agn_checkstring(L, 2);
  stm = localtime64(&t);
  if (stm == NULL) {
    lua_pushnil(L);
  } else {
    size_t reslen;
    char buff[256];
    /* str_strftime expects const struct tm *timeptr as a fourth argument, we cannot pass struct TM *stm; 7.8.4 fix */
    struct tm newtm = { 0 };
    tools_bzero(buff, sizeof(buff));
    newtm.tm_year  = stm->tm_year;
    newtm.tm_mon   = stm->tm_mon;
    newtm.tm_mday  = stm->tm_mday;
    newtm.tm_hour  = stm->tm_hour;
    newtm.tm_min   = stm->tm_min;
    newtm.tm_sec   = stm->tm_sec;
    newtm.tm_wday  = stm->tm_wday;
    newtm.tm_yday  = stm->tm_yday;
    newtm.tm_isdst = stm->tm_isdst;
    reslen = str_strftime(buff, sizeof(buff), fmt, &newtm, ms);
    if (reslen == 0) {
      luaL_error(L, "Error in " LUA_QS ": conversion result too big or other error.", "strings.strftime");
    } else {
      lua_pushstring(L, buff);
    }
  }
  return 1;
}


/* Takes a string s, usually a time stamp, and parses it using the format specified by string format.
   On success returns a table with the year, month, and optionally hour, minute, second.

   The timestamp may consist of date and time, date only or time only.

   If there are surplus characters in s, the function also returns the portion of s starting from the
   first character not processed in the function call. If there are no surplus characters, the empty
   string will be returned.

   If the function fails to match the format string, the function returns `null`. It also returns `null`
   with an unknown specifier.

   The function provides an interface to the underlying C function strptime(), but ignores the day of
   month ('%a' and '%A' specifiers). The function deliberately does not know the '%b' and '%B' specifiers
   for the name of the month and returns 'null' in this case.

   You may use '%a' or '%A' to skip all alphabetical letters and digits plus all characters with an
   ASCII code greater than 127 in s.

   The non-standard '%s' specifier denotes milliseconds. If given, they will be returned as the seventh
   entry in the resulting table.

   Examples:

   > strings.strptime("2025-08-29 12:40:44.123", "%Y-%m-%d %H:%M:%S"):
   [2025, 8, 29, 12, 40, 44]       .123

   > strings.strptime("12:40:44.123", "%H:%M:%S.%s"):
   [1900, 1, 0, 12, 40, 44, 123]

   > strings.strptime("2025-08-29", "%Y-%m-%d"):
   [2025, 8, 29, 0, 0, 0]

   > strings.strptime("äöü123 12:40:44", "%a%t%H:%M:%S"):  # ignore äöü123 ('%a') and the subsequent white space ('%t')
   [1900, 1, 0, 12, 40, 44]

   For all date and time specifiers, refer to the `os.date` description in Chapter 14.1. 5.5.7 */
static int _str_strptime (lua_State *L) {
  char *result;
  struct tm parsed_time;
  int ms, nrets = 2;
  memset(&parsed_time, 0, sizeof(struct tm));
  result = str_strptime(agn_checkstring(L, 1), agn_checkstring(L, 2), &parsed_time, &ms);
  if (result != NULL) {
    luaL_checkstack(L, 2, "not enough stack space");
    lua_createtable(L, 6 + (ms != -1), 0);
    lua_rawsetinumber(L, -1, 1, parsed_time.tm_year + 1900);
    lua_rawsetinumber(L, -1, 2, parsed_time.tm_mon + 1);
    lua_rawsetinumber(L, -1, 3, parsed_time.tm_mday);
    lua_rawsetinumber(L, -1, 4, parsed_time.tm_hour);
    lua_rawsetinumber(L, -1, 5, parsed_time.tm_min);
    lua_rawsetinumber(L, -1, 6, parsed_time.tm_sec);
    if (ms != -1) {
      lua_rawsetinumber(L, -1, 7, ms);
    }
    lua_pushstring(L, result);
  } else {
    nrets = 1;
    lua_pushnil(L);
  }
  return nrets;
}


/*
** Gets an optional ending string position from argument 'arg',
** with default value 'def'.
** Negative means back from end: clip result to [0, len]
*/
static size_t getendpos (lua_State *L, int arg, lua_Integer def,
                         size_t len) {
  lua_Integer pos = luaL_optinteger(L, arg, def);
  if (pos > (lua_Integer)len)
    return len;
  else if (pos >= 0)
    return (size_t)pos;
  else if (pos < -(lua_Integer)len)
    return 0;
  else return len + (size_t)pos + 1;
}

static int str_byte (lua_State *L) {  /* taken from Lua 5.4.6, 3.0.0 */
  size_t l;
  const char *s = luaL_checklstring(L, 1, &l);
  lua_Integer pi = luaL_optinteger(L, 2, 1);
  size_t posi = tools_posrelat(pi, l);
  size_t pose = getendpos(L, 3, pi, l);
  int n, i;
  if (posi > pose) return 0;  /* empty interval; return no values */
  if (l_unlikely(pose - posi >= (size_t)INT_MAX))  /* arithmetic overflow? */
    return luaL_error(L, "Error in " LUA_QS ": string slice too long.", "strings.byte");
  n = (int)(pose -  posi) + 1;
  luaL_checkstack(L, n, "string slice too long");
  for (i=0; i<n; i++)
    lua_pushinteger(L, uchar(s[posi+i-1]));
  return n;
}


static int str_sub (lua_State *L) {  /* taken from Lua 5.4.6, 3.1.0 */
  size_t l;
  const char *s = luaL_checklstring(L, 1, &l);
  size_t start = tools_posrelat(luaL_checkinteger(L, 2), l);
  size_t end = getendpos(L, 3, -1, l);
  if (start <= end)
    lua_pushlstring(L, s + start - 1, (end - start) + 1);
  else lua_pushliteral(L, "");
  return 1;
}


/* Idea taken from:
   "Algorithm Implementation/Strings/Dice's coefficient",
   https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Dice%27s_coefficient
   See C solution.

   The function returns the bigrams for strings s. With no option, the return is a sequence of strings of length 2 each,
   with any option a sequence with the bigrams encoded to 4-byte integers is returned. */
#define BGHASH(x,y) (((int32_t)uchar((x)) << 16) | uchar(y))

static int str_bigrams (lua_State *L) {  /* 3.10.0 */
	int i, m, nargs;
  size_t l;
  const char *s = luaL_checklstring(L, 1, &l);
  nargs = lua_gettop(L);
	m = l - 1;
  /* hash the strings to produce n-gram sets */
  agn_createseq(L, m);
  if (nargs == 1) {
    for (i=0; i < m; i++) {
      char c[2];
      c[0] = s[i];
      c[1] = s[i + 1];
      lua_pushlstring(L, c, 2);
      lua_seqrawseti(L, -2, i + 1);
    }
  } else {
    for (i=0; i < m; i++) {
      agn_seqsetinumber(L, -1, i + 1, BGHASH(s[i], s[i + 1]));
    }
  }
  return 1;
}


/* The function produces n-grams of string s. n should be a positive integer and not larger than the size of s.
   The return is a sequence of the ordered n-grams of s. See also: strings.bigrams. 3.10.1 */
static int str_ngrams (lua_State *L) {
	int i, j, m, n;
  size_t l;
  char *c;
  const char *s = luaL_checklstring(L, 1, &l);
  n = agnL_optposint(L, 2, 2);
	m = (int)l - (n - 1);
	if (m < 1)
	  luaL_error(L, "Error in " LUA_QS ": second argument greater than length of string.", "strings.ngrams");
  c = agn_malloc(L, n*CHARSIZE, "strings.ngrams", NULL);  /* 4.11.5 fix */
  agn_createseq(L, m);
  for (i=0; i < m; i++) {
    for (j=0; j < n; j++) c[j] = s[i + j];
    lua_pushlstring(L, c, n);
    lua_seqrawseti(L, -2, i + 1);
  }
  xfree(c);
  return 1;
}


/* Returns all integer indices of a table in a new table and puts it onto the top of the stack. flag = 0: no integer indices in hash part,
   flag = 1: there are integer indices in hash part, 2.30.1. https://en.cppreference.com/w/c/algorithm/qsort */
static int compare_ints (const void* a, const void* b) {
  int arg1 = *(const int*)a;
  int arg2 = *(const int*)b;
  return (arg1 > arg2) - (arg1 < arg2);
}

/* Returns the Dice's coefficient of two strings by measuring how similar a set and another set are, in terms of the number of common bigrams.
   The return is the number of matches (where each match counts twice) divided by the combined length of s and t minus 2.
   taken from: https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Dice%27s_coefficient, Java implementation; 3.10.0 */
static int str_dice (lua_State *L) {
  size_t lens, lent;
  int i, j, m, n, matches, *sPairs, *tPairs;
  const char *s = luaL_checklstring(L, 1, &lens);
  const char *t = luaL_checklstring(L, 2, &lent);
	if (s == NULL || t == NULL || lens == 0 || lent == 0) {
	  lua_pushinteger(L, 0);
		return 1;
	}
	/* quick check to catch identical objects */
	if (tools_streq(s, t)) {
	  lua_pushinteger(L, 1);
		return 1;
	}
  /* avoid exception for single character searches */
  if (tools_strlen(s) < 2 || tools_strlen(t) < 2) {
    lua_pushinteger(L, 0);
    return 1;
  }
	/* create the bigrams for string s */
	n = lens - 1;
	sPairs = agn_malloc(L, n*sizeof(int), "strings.dice", NULL);
	for (i=0; i < n; i++) {
	  sPairs[i] = BGHASH(s[i], s[i + 1]);
  }
	/* create the bigrams for string t */
	m = lent - 1;
	tPairs = agn_malloc(L, m*sizeof(int), "strings.dice", sPairs, NULL);
	for (i=0; i < m; i++) {
	  tPairs[i] = BGHASH(t[i], t[i + 1]);
	}
	/* sort the bigram lists */
	qsort(sPairs, n, sizeof(int), compare_ints);
	qsort(tPairs, m, sizeof(int), compare_ints);
	/* count the matches */
	matches = 0; i = 0; j = 0;
	while (i < n && j < m) {
		if (sPairs[i] == tPairs[j]) {
			matches += 2;
			i++;
			j++;
		} else if (sPairs[i] < tPairs[j])
			i++;
		else
			j++;
	}
	xfreeall(sPairs, tPairs);
	lua_pushnumber(L, ((lua_Number)matches/(lua_Number)(n + m)));
	return 1;
}

/* Copyright (c) 2018 Phil Leblanc  -- MIT-licenced. 3.10.1
   Taken from Phil Leblanc's luazen package, version 0.16-1.

   xor(input:string, key:string) =>  output:string
   Obfuscates a string s using binary xor and a key string k. The output has the same length as the input.
   If key k is shorter than s, it is repeated as much as necessary.
   To get back the original string, just call the function with the output and the same key again.
   See also: strings.tobytes, utils.hexlify, utils.unhexlify. */
static int str_obfusxor (lua_State *L) {
  size_t slen, klen, is, ik;
  unsigned char *p;
  const char *s, *k;
  s = luaL_checklstring(L, 1, &slen);
  k = luaL_checklstring(L, 2, &klen);
  p = (unsigned char *)agn_malloc(L, sizeof(unsigned char)*slen, "strings.obfusxor", NULL);  /* 4.11.5 fix */
  is = 0; ik = 0;
  while (is < slen) {
    p[is] = uchar(s[is]) ^ uchar(k[ik]);
    is++; ik++;
    if (ik == klen) ik = 0;
  }
  lua_pushlstring(L, (char *)p, slen);
  xfree(p);
  return 1;
}


#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

/* Computes either the Jaro similarity or Jaro-Winkler similarity, measures of two strings' similarity: the higher the value, the more
   similar the strings are. The score is normalised such that 0 equates to no similarities and 1 is an exact match.

   By default, the function takes into account that if up to four characters at the start of the strings match, the score will be higher -
   thus computing the Jaro-Winkler similarity. If option is set to `false`, the Jaro similarity is returned which does not check whether
   strings match at their beginning.

   Taken from: https://rosettacode.org/wiki/Jaro_similarity, C and C++ sections; portions tuned by Gemini AI (50% to 1000%) 7.8.12 */
static int str_jaro (lua_State *L) {
  double weight;
  size_t l1, l2;
  int i, k, start, end, matches, match_distance, option;
  Charbuf *s1_matches, *s2_matches;
  const char *s1, *s2;
  s1 = agn_checklstring(L, 1, &l1);
  s2 = agn_checklstring(L, 2, &l2);
  option = agnL_optboolean(L, 3, 1);
  if (s1 == s2 || (l1 == l2 && memcmp(s1, s2, l1) == 0)) {
    lua_pushinteger(L, 1);
    return 1;
  }
  if (l1 > MAX_INT || l2 > MAX_INT) {
    luaL_error(L, "Error in " LUA_QS ": at least one string is too long.", "strings.jaro");
  } else if (l1 == 0) {
    lua_pushinteger(L, l2 == 0 ? 1 : 0);
    return 1;
  } else if (l2 == 0) {
    lua_pushinteger(L, 0);
    return 1;
  } else if (l1 == 1 && l2 == 1) {
    lua_pushinteger(L, s1[0] == s2[0]);
    return 1;
  }
  match_distance = MAX(l1, l2) / 2 - 1;
  if (match_distance < 0) match_distance = 0;

  s1_matches = bitfield_new(L, l1, 0);
  s2_matches = bitfield_new(L, l2, 0);
  matches = 0;
  int len1 = (int)l1;
  int len2 = (int)l2;
  int lowest_unmatched_k = 0;
  for (i = 0; i < len1; i++) {
    start = MAX(lowest_unmatched_k, i - match_distance);
    end   = MIN(i + match_distance + 1, len2);
    char c1 = s1[i];
    for (k = start; k < end; k++) {
      if (!bitfield_get(L, s2_matches, k + 1) && c1 == s2[k]) {
        bitfield_set(L, s1_matches, i + 1);
        bitfield_set(L, s2_matches, k + 1);
        matches++;
        if (k == lowest_unmatched_k) {
          while (lowest_unmatched_k < len2 && bitfield_get(L, s2_matches, lowest_unmatched_k + 1)) {
            lowest_unmatched_k++;
          }
        }
        break;
      }
    }
  }
  if (matches == 0) {
    weight = 0.0;
  } else {
    double transpositions, m;
    transpositions = 0.0;
    k = 0;
    for (i = 0; i < len1; i++) {
      if (bitfield_get(L, s1_matches, i + 1)) {
        while (!bitfield_get(L, s2_matches, k + 1)) k++;
        if (s1[i] != s2[k]) transpositions += 0.5;
        k++;
      }
    }
    m = (double)matches;
    weight = (m / l1 + m / l2 + (m - transpositions) / m) / 3.0;
    if (option && weight > 0.7) {
      int j, len = len1 > len2 ? len2 : len1;
      j = (len >= 4) ? 4 : len;
      for (i = 0; (i < j && (s1[i] == s2[i])); i++);
      if (i) weight += i * 0.1 * (1.0 - weight);
    }
  }
  bitfield_free(s1_matches);
  bitfield_free(s2_matches);
  lua_pushnumber(L, weight);
  return 1;
}


/* Checks whether the string x is equal to the string y, using the collation of the system, and returns
   `true` or `false`. The function internally calls the C function strcoll() to conduct the test, see
   `strings.strcoll`. 5.5.1 */
static int str_eq (lua_State *L) {
  lua_pushboolean(L, strcoll(agn_checkstring(L, 1), agn_checkstring(L, 2)) == 0);
  return 1;
}


/* Checks whether the string x is not equal to the string y, using the collation of the system, and returns
   `true` or `false`. The function internally calls the C function strcoll() to conduct the test, see
   `strings.strcoll`. 5.5.1 */
static int str_neq (lua_State *L) {
  lua_pushboolean(L, strcoll(agn_checkstring(L, 1), agn_checkstring(L, 2)) != 0);
  return 1;
}


/* Checks whether the string x is lexicographically less than the string y, using the collation of the system,
   and returns `true` or `false`. The function internally calls the C function strcoll() to conduct the test,
   see `strings.strcoll`. 5.5.1 */
static int str_lt (lua_State *L) {
  lua_pushboolean(L, strcoll(agn_checkstring(L, 1), agn_checkstring(L, 2)) < 0);
  return 1;
}


/* Checks whether the string x is lexicographically greater than the string y, using the collation of the system,
   and returns `true` or `false`. The function internally calls the C function strcoll() to conduct the test,
   see `strings.strcoll`. 5.5.1 */
static int str_gt (lua_State *L) {
  lua_pushboolean(L, strcoll(agn_checkstring(L, 1), agn_checkstring(L, 2)) > 0);
  return 1;
}


/* Checks whether the string x is lexicographically less than or equal to the string y, using the collation of
   the system, and returns `true` or `false`. The function internally calls the C function strcoll() to conduct
   the test, see `strings.strcoll`. 5.5.1 */
static int str_le (lua_State *L) {
  lua_pushboolean(L, strcoll(agn_checkstring(L, 1), agn_checkstring(L, 2)) <= 0);
  return 1;
}


/* Checks whether the string x is lexicographically greater than or equal to the string y, using the collation of
   the system, and returns `true` or `false`. The function internally calls the C function strcoll() to conduct
   the test, see `strings.strcoll`. 5.5.1 */
static int str_ge (lua_State *L) {
  lua_pushboolean(L, strcoll(agn_checkstring(L, 1), agn_checkstring(L, 2)) >= 0);
  return 1;
}


static int _str_hextodec (lua_State *L) {
  size_t l;
  int rc = 0;
  const char *str = luaL_checklstring(L, 1, &l);
  if (!l)
    luaL_error(L, "Error in " LUA_QS ": input string is empty.", "strings.hextodec");
  char *result = str_hextodec(str, l, &rc);
  switch (rc) {
    case 1:
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "strings.hextodec");
      break;
    case 2:
      luaL_error(L, "Error in " LUA_QS ": invalid character in input string.", "strings.hextodec");
      break;
    default:
      lua_pushstring(L, result);
  }
  xfree(result);
  return 1;
}


/* Mask for a 4KB (4096 byte) page on Mac OS X */
#ifndef PAGE_SIZE_MASK
#define PAGE_SIZE_MASK 0xFFF
#endif

/* Returns 1 if the memory block starts on one page and ends on another. Generated by Gemini AI, 7.3.3 */
static int is_string_split (const char *str, size_t len) {
  if (str == NULL || len == 0) return 0;
  uintptr_t start_addr = (uintptr_t)str;
  uintptr_t end_addr   = start_addr + len - 1;
  /* If the "page part" of the addresses are different, it's a split */
  return (start_addr & ~PAGE_SIZE_MASK) != (end_addr & ~PAGE_SIZE_MASK);
}

static int str_issplit (lua_State *L) {
  size_t l;
  const char *str = luaL_checklstring(L, 1, &l);
  lua_pushboolean(L, is_string_split(str, l));
  return 1;
}


static const luaL_Reg strlib[] = {
  /* standard strings library functions */
  {"a64", str_a64},                       /* added on August 03, 2018 */
  {"advance", str_advance},               /* added on January 10, 2017 */
  {"appendmissing", str_appendmissing},   /* added on November 25, 2016 */
  {"between", str_between},               /* added on November 25, 2016 */
  {"bigrams", str_bigrams},               /* added on January 25, 2024 */
  {"byte", str_byte},                     /* added on July 04, 2023 */
  {"capitalise", str_capitalise},         /* added on February 28, 2013 */
  {"charmap", str_charmap},               /* added on February 10, 2017 */
  {"charset", str_charset},               /* added on May 06, 2018 */
  {"chomp", str_chomp},                   /* added on November 25, 2016 */
  {"chop", str_chop},                     /* added on November 25, 2016 */
  {"compare", str_compare},               /* added on March 02, 2016 */
  {"contains", str_contains},             /* added on November 30, 2016 */
  {"cut", str_cut},                       /* added on January 06, 2017 */
  {"dleven", str_dleven},                 /* added on October 10, 2011, rewritten September 25, 2016 */
  {"dice", str_dice},                     /* added on January 27, 2024 */
  {"diffs", str_diffs},                   /* added on March 08, 2015 */
  {"dump", str_dump},
  {"eq", str_eq},                         /* added on August 23, 2025 */
  {"fields", str_fields},                 /* added on November 13, 2011 */
  {"find", str_find},
  {"format", str_format},
  {"fuzzy", str_fuzzy},                   /* added on October 10, 2024 */
  {"ge", str_ge},                         /* added on August 23, 2025 */
  {"glob", _str_glob},                    /* added on February 20, 2013 */
  {"gmatch", gmatch},
  {"gseparate", str_gseparate},           /* added on January 04, 2017 */
  {"gsub", str_gsub},
  {"gt", str_gt},                         /* added on August 23, 2025 */
  {"hextodec", _str_hextodec},            /* added on November 03, 2025 */
  {"hirschberg", str_hirschberg},         /* added on August 20, 2026 */
  {"hits", str_hits},                     /* added on January 26, 2008 */
  {"include", str_include},               /* added on December 22, 2010 */
  {"instr", str_instr},                   /* added November 28, 2022 */
  {"isstarting", str_isstarting},         /* added on April 10, 2009 */
  {"isaligned", str_isaligned},           /* added on January 03, 2020 */
  {"isalpha", str_isalpha},               /* added on December 17, 2006 */
  {"isalphanumeric", str_isalphanumeric}, /* added on December 17, 2006 */
  {"isalphaspec", str_isalphaspec},       /* added on April 11, 2013 */
  {"isalphaspace", str_isalphaspace},     /* added on June 27, 2007 */
  {"isascii", str_isascii},               /* added on August 11, 2019 */
  {"isblank", str_isblank},               /* added on September 25, 2012 */
  {"iscenumeric", str_iscenumeric},       /* added on August 26, 2012 */
  {"iscomplex", str_iscomplex},           /* added on December 16, 2025 */
  {"isconsonant", str_isconsonant},       /* added on December 27, 2014 */
  {"iscontrol", str_iscontrol},           /* added on December 27, 2014 */
  {"isdia", str_isdia},                   /* added on January 12, 2017 */
  {"isending", str_isending},             /* added on April 10, 2009 */
  {"isfractional", str_isfractional},     /* added on June 17, 2024 */
  {"isgraph", str_isgraph},               /* added on August 11, 2019 */
  {"ishex", str_ishex},                   /* added on December 27, 2014 */
  {"isintegral", str_isintegral},         /* added on June 17, 2024 */
  {"isisoalpha", str_isisoalpha},         /* added on December 31, 2012 */
  {"isisolower", str_isisolower},         /* added on December 31, 2012 */
  {"isisoprint", str_isisoprint},         /* added on December 31, 2012 */
  {"isisospace", str_isisospace},         /* added on December 31, 2012 */
  {"isisoupper", str_isisoupper},         /* added on December 31, 2012 */
  {"islatin", str_islatin},               /* added on December 18, 2006 */
  {"islatinnumeric", str_islatinnumeric}, /* added on June 13, 2009 */
  {"isloweralpha", str_isloweralpha},     /* added on November 09, 2008 */
  {"islowerlatin", str_islowerlatin},     /* added on April 30, 2007 */
  {"isolower", str_isolower},             /* added on December 31, 2012 */
  {"ismagic", str_ismagic},               /* added on October 14/15, 2006 */
  {"ismultibyte", str_ismultibyte},       /* added on September 18, 2019 */
  {"isnumber", str_isnumber},             /* added on December 16, 2025 */
  {"isnumeric", str_isnumeric},           /* added on August 26, 2012 */
  {"isnumberspace", str_isnumberspace},   /* added on June 29, 2007 */
  {"isoupper", str_isoupper},             /* added on December 31, 2012 */
  {"ispattern", str_ispattern},           /* added on September 30, 2025 */
  {"isprintable", str_isprintable},       /* added on December 27, 2014 */
  {"isspace", str_isspace},               /* added on September 25, 2012 */
  {"isspec", str_isspec},                 /* added on April 11, 2013 */
  {"issubseq", str_issubseq},             /* added on April 04, 2025 */
  {"issplit", str_issplit},               /* added on March 26, 2026 */
  {"isupperalpha", str_isupperalpha},     /* added on April 19, 2009 */
  {"isupperlatin", str_isupperlatin},     /* added on April 19, 2009 */
  {"isutf8", str_isutf8},                 /* added on January 04, 2013 */
  {"isvowel", str_isvowel},               /* added on April 18, 2024 */
  {"iswrapped", str_iswrapped},           /* added on November 25, 2016 */
  {"iterate", str_iterate},               /* added on February 22, 2022 */
  {"itoa", str_itoa},                     /* added on August 11, 2018 */
  {"itouni", str_itouni},                 /* added on July 14, 2025 */
  {"jaro", str_jaro},                     /* added January 29, 2024 */
  {"join", str_join},                     /* added November 28, 2022 */
  {"lcs", str_lcs},                       /* added on April 03, 2025 */
  {"le", str_le},                         /* added on August 23, 2025 */
  {"leven", str_leven},                   /* added on April 03, 2025 */
  {"ljustify", str_ljustify},             /* added on July 20, 2026 */
  {"lower", str_lower},                   /* added on October 11, 2024 */
  {"lpad", str_rjustify},                 /* added on July 20, 2026, alias */
  {"lrtrim", str_lrtrim},                 /* added on April 30, 2013 */
  {"lt", str_lt},                         /* added on August 23, 2025 */
  {"ltrim", str_ltrim},                   /* added on June 24, 2008 */
  {"match", str_match},
  {"matches", str_matches},               /* added on June 14, 2022 */
  {"neq", str_neq},                       /* added on August 23, 2025 */
  {"ngrams", str_ngrams},                 /* added on January 30, 2024 */
  {"obfusxor", str_obfusxor},             /* added on January 29, 2024 */
  {"pack", str_pack},
  {"packsize", str_packsize},
  {"phonetiqs", str_phonetiqs},           /* added on March 09, 2015 */
  {"random", str_random},                 /* added on December 15, 2018 */
  {"remove", str_remove},                 /* added on April 10, 2009 */
  {"repeat", str_repeat},
  {"replace", str_replace},               /* added November 28, 2022 */
  {"reverse", _str_reverse},
  {"rjustify", str_rjustify},             /* added on July 20, 2026 */
  {"rotateleft", str_rotateleft},         /* added on September 26, 2019 */
  {"rotateright", str_rotateright},       /* added on September 26, 2019 */
  {"rpad", str_ljustify},                 /* added on July 20, 2026, alias */
  {"rtrim", str_rtrim},                   /* added on June 24, 2008 */
  {"separate", str_separate},             /* added on June 24, 2013 */
  {"shannon", str_shannon},               /* added on May 06, 2018 */
  {"strchr", str_strchr},                 /* added on March 04, 2022 */
  {"strcoll", str_strcoll},               /* added on March 14, 2023 */
  {"strcspn", str_strcspn},               /* added on March 14, 2023 */
  {"strftime", _str_strftime},            /* added on August 30, 2025 */
  {"strrchr", str_strrchr},               /* added on March 04, 2022 */
  {"strcmp", str_strcmp},                 /* added on March 02, 2022 */
  {"stricmp", str_stricmp},               /* added on April 17, 2023 */
  {"strlen", str_strlen},                 /* added on August 12, 2019 */
  {"strncmp", str_strncmp},               /* added on April 17, 2023 */
  {"strnstr", str_strnstr},               /* added on March 10, 2023 */
  {"strptime", _str_strptime},            /* added on August 29, 2025 */
  {"strspn", str_strspn},                 /* added on March 14, 2023 */
  {"strstr", str_strstr},                 /* added on March 04, 2022 */
  {"strtoul", str_strtoul},               /* added on May 17, 2023 */
  {"strverscmp", str_strverscmp},         /* added on March 02, 2022 */
  {"strxfrm", str_strxfrm},               /* added on August 14, 2025 */
  {"sub", str_sub},                       /* added on July 11, 2023 */
  {"tobytes", str_tobytes},
  {"tochars", str_tochars},
  {"tolatin", str_tolatin},               /* added on January 04, 2013 */
  {"tolower", str_tolower},               /* added on March 17, 2022 */
  {"toupper", str_toupper},               /* added on March 17, 2022 */
  {"toutf8", str_toutf8},                 /* added on January 04, 2013 */
  {"trim", str_trim},                     /* added on October 11, 2024 */
  {"utf8size", str_utf8size},             /* added on January 04, 2013 */
  {"transform", str_transform},           /* added on January 03, 2013 */
  {"uncapitalise", str_uncapitalise},     /* added on November 25, 2016 */
  {"unpack", str_unpack},
  {"unwrap", str_unwrap},                 /* added on April 29, 2023 */
  {"upper", str_upper},                   /* added on October 11, 2024 */
  {"walker", str_walker},                 /* added on February 10, 2023 */
  {"words", str_words},                   /* added on April 07, 2009 */
  {"wrap", str_wrap},                     /* added on November 25, 2016 */
  {NULL, NULL}
};


static void createmetatable (lua_State *L) {
  lua_createtable(L, 0, 1);  /* create metatable for strings */
  lua_pushliteral(L, "");  /* dummy string */
  lua_pushvalue(L, -2);
  lua_setmetatable(L, -2);  /* set string metatable */
  agn_poptop(L);  /* pop dummy string */
  lua_pushvalue(L, -2);  /* string library... */
  lua_setfield(L, -2, "__index");  /* ...is the __index metamethod */
  agn_poptop(L);  /* pop metatable */
}


/*
** Open string library
*/
LUALIB_API int luaopen_strings (lua_State *L) {
  luaL_register(L, LUA_STRLIBNAME, strlib);
  createmetatable(L);
  return 1;
}