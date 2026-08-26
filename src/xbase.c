/******************************************************************************
 * $Id: dbfopen.c,v 1.48 2003/03/10 14:51:27 warmerda Exp $
 *
 * Project:  Shapelib
 * Purpose:  Implementation of .dbf access API documented in dbf_api.html.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * This software is available under the following "MIT Style" license,
 * or at the option of the licensee under the LGPL (see LICENSE.LGPL).  This
 * option is discussed in more detail in shapelib.html.
 *
 * --
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log: dbfopen.c,v $
 * Revision 1.48  2003/03/10 14:51:27  warmerda
 * DBFWrite* calls now return FALSE if they have to truncate
 *
 * The xBase package, initiated June 05, 2010, written by Alexander Walz.
 * This Agena Binding is largely based on the Shapelib 1.2.10 source files, with
 * codepage, and the DBFMarkRecordDeleted and DBFIsRecordDeleted C functions taken
 * from the Shapelib 1.3.0 distribution.

 * Shapelib download site: http://download.osgeo.org/shapelib/

 * Extensions to process binary Doubles added in Agena 2.2.0 RC 3.
*/

/* Supported input for dBASE data types:

   Data Type          data input
   C (Character)      all OEM code page characters
   D (Date)           8 digits in YYYYMMDD format
   N (Numeric)        - . 0 1 2 3 4 5 6 7 8 9
   F (Float)          - . 0 1 2 3 4 5 6 7 8 9
   L (Logical)        ? Y y N n T t F f (? when not initialized).
   O (Binary Double)  stored in Little Endian byte order, dBASE Level 7.
   B (Binary)         10 digits representing a .DBT block number
   I (Binary int32_t) stored in Little Endian byte order
   T (Timestamp)      two LE int32_t's (Visual FoxPro acknowledges 'T', Visual dBASE 7 '@'.

   Proprietary extensions are:

   c (Complex)        C complex double
   b (Byte)           an unsigned byte
   f (Decimal)        C 4-byte float
   u (UShort)         C unsigned 2-byte integer
   s (Short)          C signed 2-byte integer */

/* Compared this to $Id: dbfopen.c,v 1.92 2016-12-05 18:44:08 erouault Exp $: few new features, not added here, 26/05/2018 */

#include <math.h>
#include <stdint.h>  /* for int32_t */
#include <stdlib.h>
#include <string.h>

#define xbase_c
#define LUA_LIB

#include "agena.h"
#include "agenalib.h"
#include "agncmpt.h"
#include "agnhlps.h"
#include "agnt64.h"
#include "agnxlib.h"
#include "sofa.h"
#include "xbase.h"

#ifndef lua_boxpointer
#define lua_boxpointer(L, u) \
  (*(void **)(lua_newuserdata(L, sizeof(void *))) = (u))
#endif


#ifndef FALSE
#  define FALSE   0
#  define TRUE    1
#endif

static int  nStringFieldLen = 0;
static char *pszStringField = NULL;

/* Documentation on dBASE Level 7 file layout is hard to find and sometimes it is contradictory: some claim binary doubles
   are internally stored in Big Endian format, some in Little Endian. It appears that 32-bit signed integers seem to be
   Little Endian whereas it is completely unknown how binary timestamps are really stored internally: as a (whatever)
   endian double or as two (whatever) 32-bit integers. As such, we assume that in dBASE Level 7 files we have the following
   definitions regarding binary doubles and binary timestamps, but you can change this via xbase.kernel: */
static int DoubleIsBigEndian = 1;  /* 1 = Big, 0 = Little */
static int LongIsBigEndian   = 0;  /* 1 = Big, 0 = Little */
static int TimestampIsDouble = 0;  /* 1 = double, 0 = two signed int32_t's */


/* forward declaration */
static int xbase_gc (lua_State *L);


DBFHandle *aux_gethandle (lua_State *L, int idx, const char *procname) {
  DBFHandle *hnd = (DBFHandle *)luaL_checkudata(L, idx, "xbase");
  if (*hnd == NULL)
    luaL_error(L, "Error in " LUA_QS ": invalid file handle.", procname);
  return hnd;
}


DBFHandle *aux_gethandle_noerror (lua_State *L, int idx, const char *procname) {
  DBFHandle *hnd = (DBFHandle *)luaL_checkudata(L, idx, "xbase");
  return hnd;
}


/************************************************************************/
/*                             SfRealloc()                              */
/*                                                                      */
/*      A realloc cover function that will access a NULL pointer as     */
/*      a valid input.                                                  */
/************************************************************************/

static void *SfRealloc (void *pMem, int nNewSize) {
  if (pMem == NULL) {
    return (void *)malloc(nNewSize);
  } else {
    return (void *)realloc(pMem,nNewSize);
  }
}


/************************************************************************/
/*                           DBFWriteHeader()                           */
/*                                                                      */
/*      This is called to write out the file header, and field          */
/*      descriptions before writing any actual data records.  This      */
/*      also computes all the DBFDataSet field offset/size/decimals     */
/*      and so forth values.                                            */
/************************************************************************/

static void DBFWriteHeader (DBFHandle psDBF) {
  unsigned char abyHeader[XBASE_FLDHDR_SZ];
  int i, yy, mm, dd;
  struct TM *stm;
  Time64_T t = time(NULL);
  if (!psDBF->bNoHeader) return;
  psDBF->bNoHeader = FALSE;
  /* Initialize the file header information.  */
  for (i=0; i < XBASE_FLDHDR_SZ; i++)
    abyHeader[i] = 0;
  stm = gmtime64(&t);
  if (stm == NULL) {  /* invalid date ? */
    yy = 0; mm = 5; dd = 8;
  } else {
    yy = stm->tm_year;
    mm = stm->tm_mon + 1;
    dd = stm->tm_mday;
  }
  abyHeader[0] = psDBF->dBaseVersion;  /* 2.2.0 RC 3 */
  abyHeader[1] = yy;   /* YY */
  abyHeader[2] = mm;   /* MM */
  abyHeader[3] = dd;   /* DD */
  /* date updated on close, record count preset at zero */
  abyHeader[8] = psDBF->nHeaderLength % 256;  /* number of bytes in the header (16-bit number). */
  abyHeader[9] = psDBF->nHeaderLength / 256;
  abyHeader[10] = psDBF->nRecordLength % 256;  /* number of bytes in the record (16-bit number). */
  abyHeader[11] = psDBF->nRecordLength / 256;
  abyHeader[29] = (unsigned char)(psDBF->iLanguageDriver);
  /* Write the initial 32 byte file header, and all the field descriptions. */
  _fseeki64(psDBF->fp, 0, 0);
  fwrite(abyHeader, XBASE_FLDHDR_SZ, 1, psDBF->fp);
  fwrite(psDBF->pszHeader, XBASE_FLDHDR_SZ, psDBF->nFields, psDBF->fp);
  /* Write out the newline character if there is room for it. */
  if (psDBF->nHeaderLength > 32*psDBF->nFields + 32) {
    char cNewline;
    cNewline = 0x0d;
    fwrite(&cNewline, 1, 1, psDBF->fp);
  }
}


/************************************************************************/
/*                           DBFLoadRecord()                            */
/************************************************************************/

int DBFLoadRecord (DBFHandle psDBF, int iRecord) {
  if (psDBF->nCurrentRecord != iRecord) {
    int nRecordOffset;
    if (!DBFFlushRecord(psDBF)) {
      return FALSE;
    }
    nRecordOffset =
      psDBF->nRecordLength * iRecord + psDBF->nHeaderLength;
    if (_fseeki64(psDBF->fp, nRecordOffset, SEEK_SET) != 0) {
      /* printf("fseek(%ld) failed on DBF file.\n", (long) nRecordOffset); */
      /* char szMessage[128];
         sprintf(szMessage, "fseek(%ld) failed on DBF file.\n", (long) nRecordOffset);
         psDBF->sHooks.Error(szMessage); */
      return FALSE;
    }
    if (fread(psDBF->pszCurrentRecord, psDBF->nRecordLength, 1, psDBF->fp) != 1) {
      /* printf("fread(%d) failed on DBF file.\n", psDBF->nRecordLength); */
      /* char szMessage[128];
         sprintf(szMessage, "fread(%d) failed on DBF file.\n", psDBF->nRecordLength);
         psDBF->sHooks.Error(szMessage); */
      return FALSE;
    }
    psDBF->pszCurrentRecord[psDBF->nRecordLength] = '\0';  /* better sure than sorry, 2.2.0 RC 2 extension */
    psDBF->nCurrentRecord = iRecord;
  }
  return TRUE;
}

/************************************************************************/
/*                           DBFFlushRecord()                           */
/*                                                                      */
/*      Write out the current record if there is one.                   */
/************************************************************************/

/* modified for Agena 0.32.4, patched 2.2.0 RC 2 */

int DBFFlushRecord (DBFHandle psDBF) {
  int nRecordOffset, result, i;
  if (psDBF->bCurrentRecordModified && psDBF->nCurrentRecord > -1) {
    FILE *f = psDBF->fp;
    result = 0;
    psDBF->bCurrentRecordModified = FALSE;
    nRecordOffset = psDBF->nRecordLength * psDBF->nCurrentRecord
                    + psDBF->nHeaderLength;
    if (_fseeki64(f, nRecordOffset, 0) == 0) result |= 1;
    /* if (fwrite(psDBF->pszCurrentRecord, psDBF->nRecordLength, 1, f) == 1) result |= 2; */
    for (i=0; i < psDBF->nRecordLength; i++) {  /* 2.2.0 RC 3, make sure that entire record is written,
      including embedded 0's that may be part of a binary Double */
      if (fwrite(psDBF->pszCurrentRecord++, 1, 1, f) == 1) result |= 2;
    }
    psDBF->pszCurrentRecord -= psDBF->nRecordLength;
    if (fflush(f) == 0) result |= 4;
  } else  /* nothing to do, 2.2.0 RC 2 */
    result = -1;
  return (result == -1 || result == 7);
}


static int xbase_sync (lua_State *L) {
  int res;
  DBFHandle *hnd;
  hnd = aux_gethandle(L, 1, "xbase.sync");
  res = DBFFlushRecord(*hnd);
  if (res == 1)
    lua_pushtrue(L);
  else
    lua_pushfail(L);
  return 1;
}


/************************************************************************/
/*                              DBFOpen()                               */
/*                                                                      */
/*      Open a .dbf file.                                               */
/************************************************************************/

DBFHandle SHPAPI_CALL DBFOpen (const char *pszFilename, const char *pszAccess) {
  DBFHandle psDBF;
  unsigned char *pabyBuf;
  int nFields, nHeadLen, nRecLen, iField, i;
  char *pszBasename, *pszFullname;
  /* We only allow the access strings "rb" and "r+". */
  if (!tools_streqx(pszAccess, "r", "r+", "rb", "rb+", "r+b", NULL)) return NULL;  /* 2.16.12 tweaks */
  if (tools_streq(pszAccess, "r"))  /* 2.16.12 tweak */
    pszAccess = "rb";
  if (tools_streq(pszAccess, "r+"))  /* 2.16.12 tweak */
    pszAccess = "rb+";
  /* Compute the base (layer) name. If there is any extension on the passed in
     filename we will strip it off. */
  pszBasename = (char *)malloc(tools_strlen(pszFilename) + 5);  /* 2.17.8 tweak */
  if (pszBasename == NULL) return NULL;  /* 4.11.5 fix */
  strcpy(pszBasename, pszFilename);
  for (i = tools_strlen(pszBasename) - 1;  /* 2.17.8 tweak */
    i > 0 && pszBasename[i] != '.' && pszBasename[i] != '/'
          && pszBasename[i] != '\\';  i--) {}
  if (pszBasename[i] == '.')
    pszBasename[i] = '\0';
  pszFullname = (char *)malloc(tools_strlen(pszBasename) + 5);  /* 2.17.8 tweak */
  if (pszFullname == NULL) {  /* 4.11.5 fix */
    xfree(pszBasename);
    return NULL;
  }
  sprintf(pszFullname, "%s.dbf", pszBasename);
  psDBF = (DBFHandle)calloc(1, sizeof(DBFInfo));
  if (!psDBF) {  /* 7.3.4 fix */
    xfreeall(pszFullname, pszBasename);
    return NULL;
  }
  psDBF->fp = fopen(pszFullname, pszAccess);
  if (psDBF->fp == NULL) {
    sprintf(pszFullname, "%s.DBF", pszBasename);
    psDBF->fp = fopen(pszFullname, pszAccess);
  }
  free(pszBasename);
  psDBF->filename = (char *)malloc(tools_strlen(pszFilename) + 5);  /* 2.17.8 tweak */
  if (psDBF->filename == NULL) {  /* 4.11.5 fix */
    xfree(pszFullname);
    return NULL;
  }
  strcpy(psDBF->filename, pszFullname);
  free(pszFullname);
  if (psDBF->fp == NULL) {
    xfree(psDBF);
    return NULL;
  }
  psDBF->bNoHeader = FALSE;
  psDBF->nCurrentRecord = -1;
  psDBF->bCurrentRecordModified = FALSE;
  /* Read Table Header info */
  pabyBuf = (unsigned char *)malloc(500 * CHARSIZE);
  if (pabyBuf == NULL) {
    fclose(psDBF->fp);
    free(psDBF);
    return NULL;  /* 4.11.5 fix */
  }
  if (fread(pabyBuf, 32, 1, psDBF->fp) != 1) {
    fclose(psDBF->fp);
    xfree(pabyBuf);
    xfree(psDBF);
    return NULL;
  }
  psDBF->dBaseVersion = pabyBuf[0];  /* 2.9.8 */
  psDBF->nRecords =
    pabyBuf[4] + pabyBuf[5]*256 + pabyBuf[6]*256*256 + pabyBuf[7]*256*256*256;
  psDBF->nHeaderLength = nHeadLen = pabyBuf[8] + pabyBuf[9]*256;
  psDBF->nRecordLength = nRecLen = pabyBuf[10] + pabyBuf[11]*256;
  psDBF->iLanguageDriver = pabyBuf[29];
  if (nHeadLen < 32) {
    fclose(psDBF->fp);
    xfree(pabyBuf);
    xfree(psDBF);
    return NULL;
  }
  psDBF->nFields = nFields = (nHeadLen - 32) / 32;
  psDBF->pszCurrentRecord = (char *)malloc(nRecLen + 1);  /* better sure than sorry, 2.2.0 RC 2 */
  if (psDBF->pszCurrentRecord == NULL) {  /* 4.11.5 fix */
    fclose(psDBF->fp);
    xfree(pabyBuf);
    xfree(psDBF);
    return NULL;
  }
  /* Figure out the code page from the LDID and CPG */
  psDBF->pszCodePage = (char *)malloc(500 * CHARSIZE);
  if (psDBF->pszCodePage == NULL) {  /* 4.11.5 fix */
    fclose(psDBF->fp);
    xfree(psDBF->pszCurrentRecord);
    xfree(pabyBuf);
    xfree(psDBF);
    return NULL;
  }
  sprintf(psDBF->pszCodePage, "LDID/%d", psDBF->iLanguageDriver);  /* a codepage of 0x00 means `invalid` or `undetermined` */
  psDBF->lastmodified = (pabyBuf[1] + 1900)*10000 + pabyBuf[2]*100 + pabyBuf[3];  /* added 0.32.5 */
  /* Read in Field Definitions */
  pabyBuf = (unsigned char *)SfRealloc(pabyBuf, nHeadLen);
  if (!pabyBuf) {  /* 7.7.7 K&R fix */
    fclose(psDBF->fp);
    xfree(pabyBuf);
    xfree(psDBF);
    return NULL;
  }
  psDBF->pszHeader = (char *)pabyBuf;
  _fseeki64(psDBF->fp, 32, 0);
  if (fread(pabyBuf, nHeadLen-32, 1, psDBF->fp) != 1) {
    fclose(psDBF->fp);
    xfree(pabyBuf);
    xfree(psDBF);
    return NULL;
  }
  psDBF->panFieldOffset = (int *)malloc(sizeof(int) * nFields);
  psDBF->panFieldSize = (int *)malloc(sizeof(int) * nFields);
  psDBF->panFieldDecimals = (int *)malloc(sizeof(int) * nFields);
  psDBF->pachFieldType = (char *)malloc(CHARSIZE * nFields);
  if (psDBF->panFieldOffset == NULL ||  /* 4.11.5 fix */
      psDBF->panFieldSize == NULL ||
      psDBF->panFieldDecimals == NULL ||
      psDBF->pachFieldType == NULL) {
    xfreeall(psDBF->panFieldOffset, psDBF->panFieldSize, psDBF->panFieldDecimals, psDBF->pachFieldType);
    fclose(psDBF->fp);
    free(pabyBuf);
    free(psDBF);
    return NULL;
  }
  for (iField = 0; iField < nFields; iField++) {
    unsigned char *pabyFInfo;
    pabyFInfo = pabyBuf + iField*32;
    if (pabyFInfo[11] == 'N' || pabyFInfo[11] == 'F') {
      psDBF->panFieldSize[iField] = pabyFInfo[16];
      psDBF->panFieldDecimals[iField] = pabyFInfo[17];
    } else {  /* should cover dates, 2.1.6 */
      psDBF->panFieldSize[iField] = pabyFInfo[16] + pabyFInfo[17]*256;
      psDBF->panFieldDecimals[iField] = 0;
    }
    psDBF->pachFieldType[iField] = (char)pabyFInfo[11];
    if (iField == 0)
      psDBF->panFieldOffset[iField] = 1;
    else
      psDBF->panFieldOffset[iField] =
        psDBF->panFieldOffset[iField - 1] + psDBF->panFieldSize[iField - 1];
  }
  /* check for valid xBase file (0x0d flag at end of fields block), 0.32.4 */
  if (nHeadLen > 32 && pabyBuf[nHeadLen - 33] != 0x0d) {
    fprintf(stderr, "Error in `xbase.open`: this does not seem to be a known or valid xBase file.\n\n");
    fclose(psDBF->fp);
    free(pabyBuf);
    free(psDBF);
    return NULL;
  }
  return psDBF;
}


static int xbase_open (lua_State *L) {
  const char *fn, *attr;
  DBFHandle hnd;
  fn = luaL_checkstring(L, 1);
  agn_checkvalidpath(L, fn, 0, "xbase.open");  /* 7.6.3 */
  attr = luaL_optstring(L, 2, "read");
  if (tools_streqx(attr, "write", "append", "r+", "a", "w", NULL))
    attr = "rb+";
  else if (tools_streqx(attr, "read", "r", NULL))  /* 2.16.12 tweak */
    attr = "rb";
  else
    luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", "xbase.open", attr);
  hnd = DBFOpen(fn, attr);
  if (hnd == NULL)
    luaL_error(L, "Error in " LUA_QS ": could not open " LUA_QS ".", "xbase.open", fn);
  else {
    lua_boxpointer(L, hnd);
    luaL_getmetatable(L, "xbase");
    lua_setmetatable(L, -2);
  }
  return 1;
}


static int xbase_readdbf (lua_State *L) {  /* fixed 2.2.0 RC 2; extended 2.2.0 RC 5 */
  const char *fn;
  DBFHandle hnd;
  int nrecords, nfields, i, j, multifields, *fieldinfo, offset, ncols, nargs;
  lua_Integer *cols;
  char iofailure, ismarked;
  fn = NULL;
  if (lua_isstring(L, 1)) {
    fn = lua_tostring(L, 1);
    hnd = DBFOpen(fn, "rb");
    if (hnd == NULL)
      luaL_error(L, "Error in " LUA_QS ": could not open " LUA_QS ".", "xbase.readdbf", fn);
  } else {
    hnd = *aux_gethandle(L, 1, "xbase.readdbf");
  }
  nrecords = DBFGetRecordCount(hnd);
  nfields = DBFGetFieldCount(hnd);
  if (nrecords == 0 || nfields == 0) {
    if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
    luaL_error(L, "Error in " LUA_QS ": database is empty.", "xbase.readdbf");
  }
  fieldinfo = malloc(nfields*sizeof(int));
  if (fieldinfo == NULL) {
    if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "xbase.readdbf");
  }
  agn_createseq(L, nrecords);
  for (j=0; j < nfields; j++) {
    fieldinfo[j] = DBFGetFieldInfo(hnd, j, NULL, NULL, NULL);
  }
  ncols = 0;
  nargs = lua_gettop(L);
  cols = NULL;
  if (nargs > 1 && lua_ispair(L, 2)) {
    const char *setting;
    agn_pairgeti(L, 2, 1);  /* get left-hand side */
    if (lua_type(L, -1) != LUA_TSTRING) {
      int type = lua_type(L, -1);
      lua_pop(L, 2);  /* clear stack */
      xfree(fieldinfo);
      if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
      luaL_error(L, "Error in " LUA_QS ": string expected for left-hand side, got %s.", "environ.kernel",
        lua_typename(L, type));
    }
    setting = lua_tostring(L, -1);
    if (tools_streqx(setting, "field", "fields", NULL)) {  /* 2.16.12 tweak, 7.3.3 change */
      lua_Integer x;
      ncols = 1;
      agn_pairgeti(L, 2, 2);  /* get right-hand side */
      if (agn_isnumber(L, -1)) {
        cols = malloc(1 * sizeof(lua_Integer));
        if (cols == NULL) {
          xfree(fieldinfo);
          if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
          lua_pop(L, 2);  /* pop lhs and rhs of pair */
          luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "xbase.readdbf");
        }
        x = (lua_Integer)agn_tonumber(L, -1);
        if (x < 1 || x > nfields) {
          xfreeall(fieldinfo, cols);  /* 7.3.3 fix */
          if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
          lua_pop(L, 2);  /* pop lhs and rhs of pair */
          luaL_error(L, "Error in " LUA_QS ": field %d does not exist.", "xbase.readdbf", x);
        }
        cols[0] = x - 1;
        lua_pop(L, 2);  /* pop lhs and rhs of pair */
      } else if (lua_istable(L, -1)) {
        ncols = agn_size(L, -1);
        if (ncols < 1) {
          xfree(fieldinfo);
          if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
          lua_pop(L, 2);  /* pop lhs and rhs of pair */
          luaL_error(L, "Error in " LUA_QS ", in option fields: table is empty.", "xbase.readdbf");
        }
        cols = malloc(ncols * sizeof(lua_Integer));
        if (cols == NULL) {
          xfree(fieldinfo);
          if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
          lua_pop(L, 2);  /* pop lhs and rhs of pair */
          luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "xbase.readdbf");
        }
        for (i=0; i < ncols; i++) {
          x = (lua_Integer)agn_getinumber(L, -1, i + 1);
          if (x < 1 || x > nfields) {
            xfreeall(fieldinfo, cols);
            if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
            lua_pop(L, 2);  /* pop lhs and rhs of pair */
            luaL_error(L, "Error in " LUA_QS ": field %d does not exist.", "xbase.readdbf", x);
          }
          cols[i] = x - 1;
        }
        lua_pop(L, 2);
      } else if (lua_isseq(L, -1)) {
        ncols = agn_seqsize(L, -1);
        if (ncols < 1) {
          xfree(fieldinfo);
          if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
          lua_pop(L, 2);  /* pop lhs and rhs of pair */
          luaL_error(L, "Error in " LUA_QS ", in option fields: sequence is empty.", "xbase.readdbf");
        }
        cols = malloc(ncols * sizeof(lua_Integer));
        if (cols == NULL) {
          xfree(fieldinfo);
          if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
          lua_pop(L, 2);  /* pop lhs and rhs of pair */
          luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "xbase.readdbf");
        }
        for (i=0; i < ncols; i++) {
          x = (lua_Integer)agn_seqgetinumber(L, -1, i + 1);
          if (x < 1 || x > nfields) {
            xfreeall(fieldinfo, cols);
            if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
            lua_pop(L, 2);  /* pop lhs and rhs of pair */
            luaL_error(L, "Error in " LUA_QS ": field %d does not exist.", "xbase.readdbf", x);
          }
          cols[i] = x - 1;
        }
        lua_pop(L, 2);
      } else {
        xfree(fieldinfo);
        if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
        luaL_error(L, "Error in " LUA_QS ": invalid right-hand side for fields option.", "xbase.readdbf");
      }
    } else {
      agn_poptop(L);
      xfree(fieldinfo);
      if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
      luaL_error(L, "Error in " LUA_QS ": unknown option %s.", "xbase.readdbf", setting);
    }
  } else {
    ncols = nfields;
    cols = malloc(ncols * sizeof(lua_Integer));
    if (cols == NULL) {
      xfree(fieldinfo);
      if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "xbase.readdbf");
    }
    for (i=0; i < ncols; i++) cols[i] = i;
  }
  offset = 0;
  multifields = ncols != 1;
  for (i=0; i < nrecords; i++) {
    if (DBFIsRecordDeleted(hnd, i)) {  /* ignore records marked deleted, 2.2.0 RC 2 */
      offset++;
      continue;
    }
    if (multifields) agn_createseq(L, ncols);
    for (j=0; j < ncols; j++) {
      switch (fieldinfo[cols[j]]) {
        case FTString: case FTMemo: case FTBinary: case FTOle: {
          const char *result = DBFReadStringAttribute(hnd, i, cols[j], &iofailure, &ismarked);
          if (result == NULL) {
            xfree(fieldinfo); xfree(cols);
            if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
            luaL_error(L, "Error in " LUA_QS ": no string stored in database.", "xbase.readdbf");
          }
          else if (iofailure) {
            xfree(fieldinfo); xfree(cols);
            if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
            luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.readdbf");
          }
          else if (ismarked) offset++;
          else
            lua_seqrawsetistring(L, -1, (multifields) ? j + 1 : i + 1-offset, result);
          break;
        }
        case FTDouble: {
          lua_Number result = DBFReadDoubleAttribute(hnd, i, cols[j], &iofailure, &ismarked);
          if (iofailure) {
            xfree(fieldinfo); xfree(cols);
            if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
            luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.readdbf");
          }
          else if (ismarked) offset++;
          else
            agn_seqsetinumber(L, -1, (multifields) ? j + 1 : i + 1-offset, result);
          break;
        }
        case FTBinDouble: {
          lua_Number result = DBFReadBinDoubleAttribute(hnd, i, cols[j], &iofailure, &ismarked);
          if (iofailure) {
            xfree(fieldinfo); xfree(cols);
            if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
            luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.readdbf");
          }
          else if (ismarked) offset++;
          else
            agn_seqsetinumber(L, -1, (multifields) ? j + 1 : i + 1-offset, result);
          break;
        }
        case FTBinComplex: {  /* 2.31.5 */
          lua_Number re, im;
          re = DBFReadBinComplexAttribute(hnd, i, cols[j], &iofailure, &ismarked, 0);
          im = DBFReadBinComplexAttribute(hnd, i, cols[j], &iofailure, &ismarked, sizeof(lua_Number));
          if (iofailure) {
            xfree(fieldinfo); xfree(cols);
            if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
            luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.readdbf");
          }
          else if (ismarked) offset++;
          else {
            agn_pushcomplex(L, re, im);
            lua_seqrawseti(L, -2, (multifields) ? j + 1 : i + 1-offset);
          }
          break;
        }
        case FTFloat: {
          lua_Number result = DBFReadFloatAttribute(hnd, i, cols[j], &iofailure, &ismarked);
          if (iofailure) {
            xfree(fieldinfo); xfree(cols);
            if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
            luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.readdbf");
          }
          else if (ismarked) offset++;
          else
            agn_seqsetinumber(L, -1, (multifields) ? j + 1 : i + 1-offset, result);
          break;
        }
        case FTLogical: {
          const char *result = DBFReadLogicalAttribute(hnd, i, cols[j], &iofailure, &ismarked);
          if (result == NULL) {
            xfree(fieldinfo); xfree(cols);
            if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
            luaL_error(L, "Error in " LUA_QS ": no logical value stored in database.", "xbase.readdbf");
          } else if (iofailure) {
            xfree(fieldinfo); xfree(cols);
            if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
            luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.readdbf");
          }
          else if (ismarked) offset++;
          else
            lua_seqrawsetistring(L, -1, (multifields) ? j + 1 : i + 1-offset, result);
          break;
        }
        case FTByte: {  /* 2.31.6 */
          lua_Number result = DBFReadByteAttribute(hnd, i, cols[j], &iofailure, &ismarked);
          if (iofailure) {
            xfree(fieldinfo); xfree(cols);
            if (fn != NULL) DBFClose(hnd);
            luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.readdbf");
          }
          else if (ismarked) offset++;
          else {
            agn_seqsetinumber(L, -1, (multifields) ? j + 1 : i + 1-offset, (lua_Number)result);
          }
          break;
        }
        case FTDecimal: {  /* 2.31.^6 */
          lua_Number result = DBFReadDecimalAttribute(hnd, i, cols[j], &iofailure, &ismarked);
          if (iofailure) {
            xfree(fieldinfo); xfree(cols);
            if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
            luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.readdbf");
          }
          else if (ismarked) offset++;
          else
            agn_seqsetinumber(L, -1, (multifields) ? j + 1 : i + 1-offset, result);
          break;
        }
        case FTDate: {
          const char *result = DBFReadDateAttribute(hnd, i, cols[j], &iofailure, &ismarked);
          if (result == NULL) {
            xfree(fieldinfo); xfree(cols);
            if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
            luaL_error(L, "Error in " LUA_QS ": no date value stored in database.", "xbase.readdbf");
          } else if (iofailure) {
            xfree(fieldinfo); xfree(cols);
            if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
            luaL_error(L, "Error in " LUA_QS ": could not successfully read date in database.", "xbase.readdbf");
          } else if (ismarked) offset++;
          else
            lua_seqrawsetistring(L, -1, (multifields) ? j + 1 : i + 1-offset, result);
          break;
        }
        case FTInteger: {  /* 2.9.8 */
          lua_Number result = DBFReadIntegerAttribute(hnd, i, cols[j], &iofailure, &ismarked);
          if (iofailure) {
            xfree(fieldinfo); xfree(cols);
            if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
            luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.readdbf");
          }
          else if (ismarked) offset++;
          else
            agn_seqsetinumber(L, -1, (multifields) ? j + 1 : i + 1-offset, result);
          break;
        }
        case FTUShort: {  /* 7.3.3 */
          lua_Number result = DBFReadUShortAttribute(hnd, i, cols[j], &iofailure, &ismarked);
          if (iofailure) {
            xfree(fieldinfo); xfree(cols);
            if (fn != NULL) DBFClose(hnd);
            luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.readdbf");
          }
          else if (ismarked) offset++;
          else
            agn_seqsetinumber(L, -1, (multifields) ? j + 1 : i + 1-offset, result);
          break;
        }
        case FTShort: {  /* 7.3.3 */
          lua_Number result = DBFReadShortAttribute(hnd, i, cols[j], &iofailure, &ismarked);
          if (iofailure) {
            xfree(fieldinfo); xfree(cols);
            if (fn != NULL) DBFClose(hnd);
            luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.readdbf");
          }
          else if (ismarked) offset++;
          else
            agn_seqsetinumber(L, -1, (multifields) ? j + 1 : i + 1-offset, result);
          break;
        }
        case FTTimeStamp: {  /* 2.9.8 */
          int ms;
          lua_Number result = DBFReadTimeStampAttribute(hnd, i, cols[j], &iofailure, &ismarked, &ms);
          if (iofailure) {
            xfree(fieldinfo); xfree(cols);
            if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
            luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.readdbf");
          }
          else if (ismarked) offset++;
          else {
            agn_createpairnumbers(L, result, ms);
            lua_seqrawseti(L, -2, (multifields) ? j + 1 : i + 1-offset);
          }
          break;
        }
        case FTInvalid: {  /* 2.31.3 */
          lua_seqrawsetistring(L, -1, (multifields) ? j + 1 : i + 1 - offset, "(invalid)");
          break;
        }
        default: {
          xfree(fieldinfo); xfree(cols);
          if (fn != NULL) DBFClose(hnd);  /* 2.9.8 */
          luaL_error(L, "Error in " LUA_QS ": unknown type of data.", "xbase.readdbf");
        }
      }
    }
    if (multifields) lua_seqrawseti(L, -2, i + 1 - offset);
  }
  xfree(fieldinfo); xfree(cols);
  if (fn != NULL) DBFClose(hnd);
  return 1;
}


/************************************************************************/
/*                              DBFClose()                              */
/************************************************************************/

int SHPAPI_CALL DBFClose (DBFHandle psDBF) {
  /* Write out header if not already written. */
  int result = 0;
  if (psDBF == NULL) return -1;
  if (psDBF->bNoHeader) DBFWriteHeader(psDBF);
  DBFFlushRecord(psDBF);
  /* Update last access date, and number of records if we have write access. */
  if (psDBF->bUpdated) {
    int r;
    unsigned char abyHeader[32];
    Time64_T t = time(NULL);
    struct TM *stm;
    int yy, mm, dd;
    _fseeki64(psDBF->fp, 0, 0);
    r = fread(abyHeader, 32, 1, psDBF->fp);
    (void)r;  /* prevent compiler warning */
    stm = gmtime64(&t);
    if (stm == NULL) {  /* invalid date ? */
      yy = 0; mm = 5; dd = 8;
    } else {
      yy = stm->tm_year;
      mm = stm->tm_mon + 1;
      dd = stm->tm_mday;
    }
    abyHeader[1] = yy;   /* YY */
    abyHeader[2] = mm;   /* MM */
    abyHeader[3] = dd;   /* DD */
    abyHeader[4] = psDBF->nRecords % 256;
    abyHeader[5] = (psDBF->nRecords/256) % 256;
    abyHeader[6] = (psDBF->nRecords/(256*256)) % 256;
    abyHeader[7] = (psDBF->nRecords/(256*256*256)) % 256;
    _fseeki64(psDBF->fp, 0, 0);
    fwrite(abyHeader, 32, 1, psDBF->fp);
  }
  /* Close, and free resources. */
  result = fclose(psDBF->fp);
  if (psDBF->panFieldOffset != NULL) {
    free(psDBF->panFieldOffset);
    free(psDBF->panFieldSize);
    free(psDBF->panFieldDecimals);
    free(psDBF->pachFieldType);
  }
  free(psDBF->pszHeader);
  free(psDBF->pszCurrentRecord);
  free(psDBF->filename);     /* 0.32.4 */
  free(psDBF->pszCodePage);  /* 2.1.6 */
  free(psDBF);
  if (pszStringField != NULL) {
    free(pszStringField);
    pszStringField = NULL;
    nStringFieldLen = 0;
  }
  return result;
}

static int xbase_close (lua_State *L) {
  DBFHandle *hnd = aux_gethandle(L, 1, "xbase.close");
  int result = DBFClose(*hnd);
  /* ignore closed files */
  if (*hnd != NULL) {  /* this will work */
    *hnd = NULL;
    xbase_gc(L);
  }
  lua_pushboolean(L, result == 0);
  return 1;
}


/************************************************************************/
/*                             DBFCreate()                              */
/*                                                                      */
/*      Create a new .dbf file.                                         */
/************************************************************************/

DBFHandle SHPAPI_CALL DBFCreate (const char *pszFilename) {
  DBFHandle  psDBF;
  FILE *fp;
  char *pszFullname, *pszBasename;
  int i;
  /* Compute the base (layer) name. If there is any extension on the passed in filename we will strip it off. */
  pszBasename = (char *)malloc((tools_strlen(pszFilename) + 5) * CHARSIZE);  /* 2.17.8 tweak */
  if (pszBasename == NULL) return NULL;  /* 4.11.5 fix */
  strcpy(pszBasename, pszFilename);
  for (i = tools_strlen(pszBasename) - 1;  /* 2.17.8 tweak */
    i > 0 && pszBasename[i] != '.' && pszBasename[i] != '/'
          && pszBasename[i] != '\\';
    i--) {}
  if (pszBasename[i] == '.') pszBasename[i] = '\0';
  pszFullname = (char *)malloc((tools_strlen(pszBasename) + 5) * CHARSIZE);  /* 2.17.8 tweak */
  if (pszFullname == NULL) {  /* 4.11.5 fix */
    xfree(pszBasename);
    return NULL;
  }
  sprintf(pszFullname, "%s.dbf", pszBasename);
  free(pszBasename);
  /* Create the file. */
  fp = fopen(pszFullname, "wb");
  if (fp == NULL) {
    free(pszFullname);
    return NULL;
  }
  fputc(0, fp);
  fclose(fp);
  fp = fopen(pszFullname, "rb+");
  if (fp == NULL) {
    free(pszFullname);
    return NULL;
  }
  /* Create the info structure. */
  psDBF = (DBFHandle)malloc(sizeof(DBFInfo));
  if (psDBF == NULL) {
    free(pszFullname);
    fclose(fp);
    return NULL;
  }
  psDBF->fp = fp;
  psDBF->nRecords = 0;
  psDBF->nFields = 0;
  psDBF->nRecordLength = 1;
  psDBF->nHeaderLength = 33;
  psDBF->panFieldOffset = NULL;
  psDBF->panFieldSize = NULL;
  psDBF->panFieldDecimals = NULL;
  psDBF->pachFieldType = NULL;
  psDBF->pszHeader = NULL;
  psDBF->nCurrentRecord = -1;
  psDBF->bCurrentRecordModified = FALSE;
  psDBF->pszCurrentRecord = NULL;
  psDBF->bNoHeader = TRUE;
  psDBF->filename = (char *)malloc(CHARSIZE);  /* 0.32.4, 2.1.6 fix: just malloc, to be freed later in xbase.new via DBFClose */
  if (psDBF->filename == NULL) {  /* 4.11.5 fix */
    fclose(fp);
    xfree(pszFullname);
    xfree(psDBF);
    return NULL;
  }
  psDBF->pszCodePage = (char *)malloc(CHARSIZE);  /* 2.1.6, just malloc, to be freed later in xbase.new via DBFClose */
  if (psDBF->pszCodePage == NULL) {  /* 4.11.5 fix */
    fclose(fp);
    xfree(pszFullname);
    xfree(psDBF->filename);
    xfree(psDBF);
    return NULL;
  }
  psDBF->iLanguageDriver = 0x00;
  free(pszFullname);
  return psDBF;
}


#define isNumber(t)    (tools_streqx((t), "number", "Numeric", "Number", "N", NULL))
#define isFloat(t)     (tools_streqx((t), "float", "Float", "F", NULL))
#define isChar(t)      (tools_streqx((t), "string", "String", "character", "Character", "C", NULL))
#define isDate(t)      (tools_streqx((t), "date", "Date", "D", NULL))
#define isDouble(t)    (tools_streqx((t), "double", "Double", "O", NULL))
#define isComplex(t)   (tools_streqx((t), "complex", "Complex", "c", NULL))
#define isBinary(t)    (tools_streqx((t), "B", NULL))
#define isOle(t)       (tools_streqx((t), "G", NULL))
#define isMemo(t)      (tools_streqx((t), "M", NULL))
#define isInteger(t)   (tools_streqx((t), "long", "Long", "I", "integer", "Integer", NULL))
#define isUShort(t)    (tools_streqx((t), "ushort", "UShort", "u", NULL))
#define isShort(t)     (tools_streqx((t), "short", "Short", "s", NULL))
#define isTimeStamp(t) (tools_streqx((t), "timestamp", "Timestamp", "@", NULL))
#define isByte(t)      (tools_streqx((t), "byte", "Byte", "b", NULL))
#define isDecimal(t)   (tools_streqx((t), "decimal", "Decimal", "f", NULL))
#define isLogical(t)   (tools_streqx((t), "logical", "Logical", "boolean", "Boolean", "L", NULL))

static int xbase_new (lua_State *L) {
  const char *fn, *fieldname, *typename;
  int i, n, nargs, pop, versiongiven;
  DBFHandle hnd;
  fn = luaL_checkstring(L, 1);
  agn_checkvalidpath(L, fn, 0, "xbase.new");  /* 7.6.3 */
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  versiongiven = 0;
  hnd = DBFCreate(fn);
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": need at least one field.", "xbase.new", fn);
  if (hnd == NULL)
    luaL_error(L, "Error in " LUA_QS ": creating " LUA_QS " failed, file may be open or path is invalid.", "xbase.new", fn);
  hnd->dBaseVersion = 0x03;  /* default version is: dBASE III PLUS. For values, see xbase.h file */
  for (i=2; i <= nargs; i++) {
    pop = 3;
    if (!lua_ispair(L, i))
      luaL_error(L, "Error in " LUA_QS ": pair expected for argument #%d.", "xbase.new", i);
    agn_pairgeti(L, i, 1);  /* get lhs */
    if (lua_type(L, -1) != LUA_TSTRING)
      luaL_error(L, "Error in " LUA_QS ": left-hand side of pair must be a string.", "xbase.new");
    fieldname = agn_tostring(L, -1);
    if (tools_streq(fieldname, "codepage")) {  /* 2.16.12 tweak */
      int codepage;
      if (nargs == 2)
        luaL_error(L, "Error in " LUA_QS ": at least one field expected.", "xbase.new");
      agn_pairgeti(L, i, 2);  /* get rhs */
      if (lua_type(L, -1) != LUA_TNUMBER)
        luaL_error(L, "Error in " LUA_QS ": number for codepage expected.", "xbase.new");
      codepage = agn_tonumber(L, -1);
      if (codepage < 0 || codepage > 255)
        luaL_error(L, "Error in " LUA_QS ": codepage must be in [0, 255].", "xbase.new");
      hnd->iLanguageDriver = codepage;
      agn_poptoptwo(L);
      continue;
    }
    if (tools_streq(fieldname, "version")) {  /* 2.9.8, 2.16.12 tweak */
      int version;
      if (nargs == 2)
        luaL_error(L, "Error in " LUA_QS ": at least one field expected.", "xbase.new");
      agn_pairgeti(L, i, 2);  /* get rhs */
      if (lua_type(L, -1) != LUA_TNUMBER)
        luaL_error(L, "Error in " LUA_QS ": version number expected.", "xbase.new");
      version = agn_tonumber(L, -1);
      if (version < 0 || version > 255)
        luaL_error(L, "Error in " LUA_QS ": version number must be in [0, 255].", "xbase.new");
      hnd->dBaseVersion = version;
      versiongiven = 1;
      agn_poptoptwo(L);
      continue;
    }
    agn_pairgeti(L, i, 2);  /* get rhs */
    if (lua_ispair(L, -1)) {
      agn_pairgeti(L, -1, 1);  /* get lhs (2nd pair) */
      if (lua_type(L, -1) != LUA_TSTRING)
        luaL_error(L, "Error in " LUA_QS ": left-hand side of second pair must be a string.", "xbase.new");
      typename = agn_tostring(L, -1);
      agn_pairgeti(L, -2, 2);  /* get rhs (2nd pair) */
      if (lua_type(L, -1) != LUA_TNUMBER)
        luaL_error(L, "Error in " LUA_QS ": right-hand side of second pair must be a number.", "xbase.new");
      n = (int)agn_tonumber(L, -1);
      agn_poptop(L);  /* pop rhs of 2nd pair */
    } else if (lua_type(L, -1) == LUA_TSTRING) {
      typename = agn_tostring(L, -1);
      if (isNumber(typename))
        n = 15;  /* scale = number of digits following the decimal point */
      else if (isFloat(typename))
        n = 18;  /* scale = number of digits following the decimal point */
      else if (isChar(typename))
        n = 64;
      else if (isDate(typename))
        n = 8;  /* 2.1.6 */
      else if (isDouble(typename)) {  /* binary 8-byte double, 2.2.0 RC 3/2.31.5, change to dBASE Level 7, but only change if version
        options has not been given */
        n = 8;
        if (!versiongiven && hnd->dBaseVersion != 0x07);  /* formerly 0x30 for Visual FoxPro */
      }
      else if (isComplex(typename)) {  /* binary 16-byte double complex, 2.31.6, mandatorily change to agenaBASE Level 1 */
        n = 16;
        hnd->dBaseVersion = 0x07;
      }
      else if (isDecimal(typename)) {  /* binary 4-byte float (experimental), 2.31.6, mandatorily change to agenaBASE Level 1 */
        n = 4;
        hnd->dBaseVersion = 0x07;
      }
      else if (isByte(typename)) {  /* unsigned char, 2.31.6, mandatorily change to agenaBASE Level 1  */
        n = 1;
        hnd->dBaseVersion = 0x07;
      }
      else if (isUShort(typename) || isShort(typename)) {  /* unsigned 16-bit integer, 7.3.6  */
        n = 2;
        hnd->dBaseVersion = 0x07;
      }
      else if (isInteger(typename))
        n = 4;  /* 2.9.8 */
      else if (isTimeStamp(typename)) {
        n = 8;  /* 2.9.8 */
        if (!versiongiven && hnd->dBaseVersion != 0x07) hnd->dBaseVersion = 0x04;  /* dBASE Level 7, 3.31.5 */
      }
      else if (isBinary(typename) || isOle(typename) || isMemo(typename))  /* .DBT block number, 2.31.4/5 fix */
        n = 10;
      else n = 1;  /* Logical value */
      pop = 2;
    } else {
      typename = NULL; n = 0; /* to avoid compiler warnings */
      luaL_error(L, "Error in " LUA_QS ": right-hand side of pair must be a string or a pair.", "xbase.new");
    }
    if (isChar(typename)) {
      if (n < 1 || n > 254)
        luaL_error(L, "Error in " LUA_QS ": string size not in [1, 254].", "xbase.new");
      if (DBFAddField(hnd, fieldname, FTString, n, 0) == -1)
        luaL_error(L, "Error in " LUA_QS ": adding Character field to " LUA_QS " failed.", "xbase.new", fn);
      lua_pop(L, pop);
    } else if (isNumber(typename)) {
      if (n < 0 || n > 15)
        luaL_error(L, "Error in " LUA_QS ": scale not in [0, 15].", "xbase.new");
      if (DBFAddField(hnd, fieldname, FTDouble, 19, n) == -1)
        luaL_error(L, "Error in " LUA_QS ": adding Number field to " LUA_QS " failed.", "xbase.new", fn);
      lua_pop(L, pop);
    } else if (isFloat(typename)) {
      if (n < 0 || n > 18)
        luaL_error(L, "Error in " LUA_QS ": scale not in [0, 18].", "xbase.new");
      if (DBFAddField(hnd, fieldname, FTFloat, 20, n) == -1)
        luaL_error(L, "Error in " LUA_QS ": adding Float field to " LUA_QS " failed.", "xbase.new", fn);
      lua_pop(L, pop);
    } else if (isLogical(typename)) {  /* 2.2.0 RC 3, 2.16.12 tweak */
      if (DBFAddField(hnd, fieldname, FTLogical, 1, 0) == -1)
        luaL_error(L, "Error in " LUA_QS ": adding Logical field to " LUA_QS " failed.", "xbase.new", fn);
      lua_pop(L, pop);
    } else if (isDate(typename)) {
      if (DBFAddField(hnd, fieldname, FTDate, 8, 0) == -1)
        luaL_error(L, "Error in " LUA_QS ": adding Date field to " LUA_QS " failed.", "xbase.new", fn);
      lua_pop(L, pop);
    } else if (isBinary(typename) || isOle(typename) || isMemo(typename)) {  /* 2.31.4/5 fix */
      if (DBFAddField(hnd, fieldname, FTDate, 10, 0) == -1)
        luaL_error(L, "Error in " LUA_QS ": adding .DBT reference to " LUA_QS " failed.", "xbase.new", fn);
      lua_pop(L, pop);
    } else if (isDouble(typename)) {
      if (sizeof(uint64_t) != 8 || sizeof(lua_Number) != 8)
        luaL_error(L, "Error in " LUA_QS ": cannot represent binary Double field in " LUA_QS " .", "xbase.new", fn);
      if (DBFAddField(hnd, fieldname, FTBinDouble, 8, 0) == -1)
        luaL_error(L, "Error in " LUA_QS ": adding double field to " LUA_QS " failed.", "xbase.new", fn);
      lua_pop(L, pop);
    } else if (isComplex(typename)) {
      if (sizeof(uint64_t) != 8 || sizeof(lua_Number) != 8)
        luaL_error(L, "Error in " LUA_QS ": cannot represent binary Complex field in " LUA_QS " .", "xbase.new", fn);
      if (DBFAddField(hnd, fieldname, FTBinComplex, 16, 0) == -1)
        luaL_error(L, "Error in " LUA_QS ": adding double complex field to " LUA_QS " failed.", "xbase.new", fn);
      lua_pop(L, pop);
    } else if (isByte(typename)) {
      if (sizeof(unsigned char) != 1)
        luaL_error(L, "Error in " LUA_QS ": cannot represent byte field in " LUA_QS " .", "xbase.new", fn);
      if (DBFAddField(hnd, fieldname, FTByte, 1, 0) == -1)
        luaL_error(L, "Error in " LUA_QS ": adding byte field to " LUA_QS " failed.", "xbase.new", fn);
      lua_pop(L, pop);
    } else if (isInteger(typename)) {  /* 2.9.8 */
      if (sizeof(int32_t) != 4)
        luaL_error(L, "Error in " LUA_QS ": cannot represent binary long field in " LUA_QS " .", "xbase.new", fn);
      if (DBFAddField(hnd, fieldname, FTInteger, 4, 0) == -1)
        luaL_error(L, "Error in " LUA_QS ": adding binary long field to " LUA_QS " failed.", "xbase.new", fn);
      lua_pop(L, pop);
    } else if (isUShort(typename)) {  /* 7.3.3 */
      if (sizeof(uint16_t) != 2)
        luaL_error(L, "Error in " LUA_QS ": cannot represent binary unsigned short field in " LUA_QS " .", "xbase.new", fn);
      if (DBFAddField(hnd, fieldname, FTUShort, 2, 0) == -1)
        luaL_error(L, "Error in " LUA_QS ": adding binary unsigned short field to " LUA_QS " failed.", "xbase.new", fn);
      lua_pop(L, pop);
    } else if (isShort(typename)) {  /* 7.3.3 */
      if (sizeof(int16_t) != 2)
        luaL_error(L, "Error in " LUA_QS ": cannot represent binary signed short field in " LUA_QS " .", "xbase.new", fn);
      if (DBFAddField(hnd, fieldname, FTShort, 2, 0) == -1)
        luaL_error(L, "Error in " LUA_QS ": adding binary signed short field to " LUA_QS " failed.", "xbase.new", fn);
      lua_pop(L, pop);
    } else if (isDecimal(typename)) {  /* 2.31.6 */
      if (sizeof(uint32_t) != 4)
        luaL_error(L, "Error in " LUA_QS ": cannot represent binary decimal field in " LUA_QS " .", "xbase.new", fn);
      if (DBFAddField(hnd, fieldname, FTDecimal, 4, 0) == -1)
        luaL_error(L, "Error in " LUA_QS ": adding binary decimal field to " LUA_QS " failed.", "xbase.new", fn);
      lua_pop(L, pop);
    } else if (isTimeStamp(typename)) {  /* 2.9.8 */
      if (sizeof(int32_t) != 4)
        luaL_error(L, "Error in " LUA_QS ": cannot represent Timestamp field in " LUA_QS " .", "xbase.new", fn);
      if (DBFAddField(hnd, fieldname, FTTimeStamp, 8, 0) == -1)
        luaL_error(L, "Error in " LUA_QS ": adding timestamp field to " LUA_QS " failed.", "xbase.new", fn);
      lua_pop(L, pop);
    } else
      luaL_error(L, "Error in " LUA_QS ": unknown field type " LUA_QS " to be added to " LUA_QS ".",
        "xbase.new", typename, fn);
  }
  DBFClose(hnd);
  lua_pushnil(L);
  return 1;
}

/************************************************************************/
/*                            DBFAddField()                             */
/*                                                                      */
/*      Add a field to a newly created .dbf file before any records     */
/*      are written.                                                    */
/************************************************************************/

int SHPAPI_CALL DBFAddField (DBFHandle psDBF, const char *pszFieldName,
            DBFFieldType eType, int nWidth, int nDecimals) {
  char *pszFInfo;
  int i;
  size_t strlenpszFieldName;
  /* Do some checking to ensure we can add records to this file. */
  if (psDBF->nRecords > 0)
    return -1;
  if (!psDBF->bNoHeader)
    return -1;
  if ((eType != FTDouble && eType != FTFloat) && nDecimals != 0)  /* 2.1.6 */
    return -1;
  if (nWidth < 1)
    return -1;
  /* SfRealloc all the arrays larger to hold the additional field information. */
  psDBF->nFields++;
  psDBF->panFieldOffset = (int *)SfRealloc(psDBF->panFieldOffset, sizeof(int) * psDBF->nFields);
  if (!psDBF->panFieldOffset) return -1;
  psDBF->panFieldSize = (int *)SfRealloc(psDBF->panFieldSize, sizeof(int) * psDBF->nFields);
  if (!psDBF->panFieldSize) return -1;
  psDBF->panFieldDecimals = (int *)SfRealloc(psDBF->panFieldDecimals, sizeof(int) * psDBF->nFields);
  if (!psDBF->panFieldDecimals) return -1;
  psDBF->pachFieldType = (char *)SfRealloc(psDBF->pachFieldType, CHARSIZE * psDBF->nFields);
  if (!psDBF->pachFieldType) return -1;
  /* Assign the new field information fields. */
  psDBF->panFieldOffset[psDBF->nFields - 1] = psDBF->nRecordLength;
  psDBF->nRecordLength += nWidth;
  psDBF->panFieldSize[psDBF->nFields - 1] = nWidth;
  psDBF->panFieldDecimals[psDBF->nFields - 1] = nDecimals;
  if (eType == FTLogical)
    psDBF->pachFieldType[psDBF->nFields - 1] = 'L';
  else if (eType == FTString)
    psDBF->pachFieldType[psDBF->nFields - 1] = 'C';
  else if (eType == FTInteger)
    psDBF->pachFieldType[psDBF->nFields - 1] = 'I';  /* 2.9.8 */
  else if (eType == FTUShort)
    psDBF->pachFieldType[psDBF->nFields - 1] = 'u';  /* 7.3.3 */
  else if (eType == FTShort)
    psDBF->pachFieldType[psDBF->nFields - 1] = 's';  /* 7.3.3 */
  else if (eType == FTShort)
    psDBF->pachFieldType[psDBF->nFields - 1] = 's';  /* 7.3.3 */
  else if (eType == FTFloat)
    psDBF->pachFieldType[psDBF->nFields - 1] = 'F';  /* 2.1.6 */
  else if (eType == FTBinDouble)  /* 2.31.3 */
    psDBF->pachFieldType[psDBF->nFields - 1] = 'O';  /* 2.2.0 RC 3, 2.31.3 changed from 'B' to 'O' */
  else if (eType == FTBinComplex)  /* 2.31.6 */
    psDBF->pachFieldType[psDBF->nFields - 1] = 'c';
  else if (eType == FTBinary)  /* 2.31.3 changed from FTBinDouble */
    psDBF->pachFieldType[psDBF->nFields - 1] = 'B';  /* 2.2.0 RC 3 */
  else if (eType == FTDate)
    psDBF->pachFieldType[psDBF->nFields - 1] = 'D';  /* 2.1.6 */
  else if (eType == FTTimeStamp)
    psDBF->pachFieldType[psDBF->nFields - 1] = '@';  /* 2.9.8, 2.31.3 changed from T to @ */
  else if (eType == FTDouble)  /* 2.31.3 */
    psDBF->pachFieldType[psDBF->nFields - 1] = 'N';
  else if (eType == FTMemo)  /* 2.31.3 */
    psDBF->pachFieldType[psDBF->nFields - 1] = 'M';
  else if (eType == FTOle)  /* 2.31.3 */
    psDBF->pachFieldType[psDBF->nFields - 1] = 'G';
  else if (eType == FTByte)  /* 2.31.6 */
    psDBF->pachFieldType[psDBF->nFields - 1] = 'b';
  else if (eType == FTDecimal)  /* 2.31.6 */
    psDBF->pachFieldType[psDBF->nFields - 1] = 'f';
  else
    psDBF->pachFieldType[psDBF->nFields - 1] = 'C';
  /* Extend the required header information. */
  psDBF->nHeaderLength += 32;
  psDBF->bUpdated = FALSE;
  psDBF->pszHeader = (char *)SfRealloc(psDBF->pszHeader, psDBF->nFields*32);
  if (!psDBF->pszHeader) return -1;
  pszFInfo = psDBF->pszHeader + 32 * (psDBF->nFields - 1);
  for (i = 0; i < 32; i++)
    pszFInfo[i] = '\0';
  strlenpszFieldName = tools_strlen(pszFieldName);  /* 2.17.8 tweak */
  if (strlenpszFieldName < 10)
    strncpy(pszFInfo, pszFieldName, strlenpszFieldName);
  else
    strncpy(pszFInfo, pszFieldName, 10);
  pszFInfo[11] = psDBF->pachFieldType[psDBF->nFields - 1];
  if (eType == FTString) {
    pszFInfo[16] = nWidth % 256;
    pszFInfo[17] = nWidth / 256;
  } else {
    pszFInfo[16] = nWidth;
    pszFInfo[17] = nDecimals;
  }
  /* Make the current record buffer appropriately larger. */
  psDBF->pszCurrentRecord = (char *)SfRealloc(psDBF->pszCurrentRecord, psDBF->nRecordLength);
  if (!psDBF->pszCurrentRecord) return -1;
  return psDBF->nFields - 1;
}

/************************************************************************/
/*                          DBFReadAttribute()                          */
/*                                                                      */
/*      Read one of the attribute fields of a record.                   */
/************************************************************************/

#if BYTE_ORDER == BIG_ENDIAN
#define aux_readtodouble(var,val) { \
  var = (DoubleIsBigEndian) ? \
        tools_uint64todouble(val) : \
        tools_uint64todoubleandswap(val); \
}
#else
#define aux_readtodouble(var,val) { \
  var = (DoubleIsBigEndian) ? \
        tools_uint64todoubleandswap(val) : \
        tools_uint64todouble(val); \
}
#endif

static void *DBFReadAttribute (DBFHandle psDBF, int hEntity, int iField, char chReqType, char *iofailure, char *ismarked, int offset) {  /* extended 2.2.0 RC 2 */
  int nRecordOffset;
  unsigned char *pabyRec;
  void *pReturnField = NULL;
  static double dDoubleField;
  *ismarked = *iofailure = 0;
  /* Verify selection. */
  if (hEntity < 0 || hEntity >= psDBF->nRecords) return NULL;
  if (iField < 0 || iField >= psDBF->nFields) return NULL;
  /* Have we read the record ? */
  if (psDBF->nCurrentRecord != hEntity) {
    int i;
    DBFFlushRecord(psDBF);
    nRecordOffset = psDBF->nRecordLength * hEntity + psDBF->nHeaderLength;
    if (_fseeki64(psDBF->fp, nRecordOffset, 0) != 0) {
      fprintf(stderr, "Error in " LUA_QS ": _fseeki64(%d) failed on DBF file.\n",
                      "xbase", nRecordOffset);
      *iofailure = 1;
      return NULL;
    }
    /* make sure that the entire record is read, including embedded 0's that may be part of a
       binary Double, 2.2.0 RC 3 */
    for (i=0; i < psDBF->nRecordLength; i++) {
      if (fread(psDBF->pszCurrentRecord++, 1, 1, psDBF->fp) != 1) {
        fprintf(stderr, "Error in " LUA_QS ": fread(%d) failed on DBF file.\n",
          "xbase", psDBF->nRecordLength);
        *iofailure = 1;
        return NULL;
      }
    }
    psDBF->pszCurrentRecord -= psDBF->nRecordLength;
    psDBF->nCurrentRecord = hEntity;
  }
  pabyRec = (unsigned char *) psDBF->pszCurrentRecord;
  if (*pabyRec == '*') *ismarked = 1;  /* record has been marked as deleted ?  Return it anyway. 2.2.0 RC 2 */
  /* Ensure our field buffer is large enough to hold this buffer. */
  if (psDBF->panFieldSize[iField] - offset + 1 > nStringFieldLen) {
    nStringFieldLen = (psDBF->panFieldSize[iField] - offset)*2 + 10;
    pszStringField = (char *)SfRealloc(pszStringField, nStringFieldLen);
  }
  /* if (psDBF->panFieldSize[iField] + 1 > nStringFieldLen) {
    nStringFieldLen = psDBF->panFieldSize[iField]*2 + 10;  // ! Do NOT subtract offset as this will lead to lost bytes !
    pszStringField = (char *)SfRealloc(pszStringField, nStringFieldLen);
  } */
  /* Extract the requested field. */
  tools_memcpy(pszStringField,  /* copy the entire record including embedded 0's that may constiture a binary Double, 2.2.0 RC 3, 2.21.5 tweak */
    ((const char *)pabyRec) + psDBF->panFieldOffset[iField] + offset,
    psDBF->panFieldSize[iField] - offset);
  pszStringField[psDBF->panFieldSize[iField] - offset] = '\0';
  pReturnField = pszStringField;
  /* Decode the field. */
  if (chReqType == 'N' || chReqType == 'F') {  /* 2.1.6 */
    if ((strstr(pszStringField, "#QNAN") - pszStringField + 1 >= 0) ||
        (strstr(pszStringField, "#NAN") - pszStringField + 1 >= 0) ||
        (strstr(pszStringField, "#IND") - pszStringField + 1 >= 0))  /* 2.9.8 fix; NaN found in field ? */
      dDoubleField = AGN_NAN;
    else if (strstr(pszStringField, "#INF") - pszStringField + 1 >= 0)  /* 2.9.8 fix; else INF ? */
      dDoubleField = atof(pszStringField) * HUGE_VAL;
    else
      dDoubleField = atof(pszStringField);
    pReturnField = &dDoubleField;
  } else if (chReqType == 'O') {  /* (complex) Double, 8-byte, Big Endian, 2.2.0 RC 3, changed from B to O in 2.31.5 */
    uint64_t ulong;
    tools_memcpy(&ulong, pszStringField, sizeof(uint64_t));  /* 2.21.5 tweak */
    aux_readtodouble(dDoubleField, ulong);
    pReturnField = &dDoubleField;
  } else if (chReqType == 'c') {  /* (complex) Double, 8-byte, Big Endian, 2.2.0 RC 3, changed from B to O in 2.31.5 */
    uint64_t ulong;
    tools_memcpy(&ulong, pszStringField, sizeof(uint64_t));  /* 2.21.5 tweak */
#if BYTE_ORDER == BIG_ENDIAN
    dDoubleField = tools_uint64todoubleandswap(ulong);
#else
    dDoubleField = tools_uint64todouble(ulong);
#endif
    pReturnField = &dDoubleField;
  } else if (chReqType == 'b') {  /* unsigned char, 2.31.6 */
    unsigned char slong;
    tools_memcpy(&slong, pszStringField, sizeof(unsigned char));
    dDoubleField = (lua_Number)slong;
    pReturnField = &dDoubleField;
  } else if (chReqType == 'I') {  /* Long, 32-bit signed integer, Little Endian, 2.9.8 */
    int32_t slong;
    tools_memcpy(&slong, pszStringField, sizeof(int32_t));  /* 2.21.5 tweak */
#if BYTE_ORDER == BIG_ENDIAN
    if (!LongIsBigEndian) slong = tools_swapint32(slong);
#else
    if (LongIsBigEndian)  slong = tools_swapint32(slong);
#endif
    dDoubleField = tools_sint2double(slong);
    pReturnField = &dDoubleField;
  } else if (chReqType == 'u') {  /* Unsigned Short, 16-bit unsigned integer, Little Endian, 7.3.3 */
    uint16_t sshort;
    tools_memcpy(&sshort, pszStringField, sizeof(uint16_t));
#if BYTE_ORDER == BIG_ENDIAN
    if (!LongIsBigEndian) sshort = tools_swapuint16(sshort);
#else
    if (LongIsBigEndian)  sshort = tools_swapuint16(sshort);
#endif
    dDoubleField = tools_ushort2double(sshort);
    pReturnField = &dDoubleField;
  } else if (chReqType == 's') {  /* Signed Short, 16-bit unsigned integer, Little Endian, 7.3.3 */
    int16_t sshort;
    tools_memcpy(&sshort, pszStringField, sizeof(int16_t));
#if BYTE_ORDER == BIG_ENDIAN
    if (!LongIsBigEndian) sshort = tools_swapint16(sshort);
#else
    if (LongIsBigEndian)  sshort = tools_swapint16(sshort);
#endif
    dDoubleField = tools_short2double(sshort);
    pReturnField = &dDoubleField;
  } else if (chReqType == 'f') {  /* float, 32-bit signed float, Little Endian, 2.31.6 */
    uint32_t ulong;
    tools_memcpy(&ulong, pszStringField, sizeof(uint32_t));
    dDoubleField = tools_uint32todouble(ulong);
    pReturnField = &dDoubleField;
  } /* Timestamp: read as two int32_t's */
  /* Should we trim white space off the string attribute value ? */
#ifdef TRIM_DBF_WHITESPACE
  else {
    char *pchSrc, *pchDst;
    pchDst = pchSrc = pszStringField;
    while (*pchSrc == ' ') pchSrc++;
    while (*pchSrc != '\0') *(pchDst++) = *(pchSrc++);
    *pchDst = '\0';
    while( pchDst != pszStringField && *(--pchDst) == ' ') *pchDst = '\0';
  }
#endif
  return pReturnField;
}


/************************************************************************/
/*                        DBFReadDoubleAttribute()                      */
/*                                                                      */
/*      Read a double attribute.                                        */
/************************************************************************/

/* failure parameter added for Agena binding 0.32.4, 06.06.2010 */

double SHPAPI_CALL DBFReadDoubleAttribute (DBFHandle psDBF, int iRecord, int iField, char *iofailure, char *ismarked) {
  double *pdValue;
  pdValue = (double *)DBFReadAttribute(psDBF, iRecord, iField, 'N', iofailure, ismarked, 0);
  if (pdValue == NULL)
    return 0.0;
  else
    return *pdValue;
}


double SHPAPI_CALL DBFReadFloatAttribute (DBFHandle psDBF, int iRecord, int iField, char *iofailure, char *ismarked) {  /* 2.1.6 */
  double *pdValue;
  pdValue = (double *)DBFReadAttribute(psDBF, iRecord, iField, 'F', iofailure, ismarked, 0);
  if (pdValue == NULL)
    return 0.0;
  else
    return *pdValue;
}


double SHPAPI_CALL DBFReadBinDoubleAttribute (DBFHandle psDBF, int iRecord, int iField,
  char *iofailure, char *ismarked) {  /* 2.2.0 RC 3 */
  double *pdValue;
  pdValue = (double *)DBFReadAttribute(psDBF, iRecord, iField, 'O', iofailure, ismarked, 0);  /* changed from B to O 2.31.5 */
  if (pdValue == NULL)
    return 0.0;
  else
    return *pdValue;
}


double SHPAPI_CALL DBFReadBinComplexAttribute (DBFHandle psDBF, int iRecord, int iField,
  char *iofailure, char *ismarked, int offset) {  /* 2.2.0 RC 3 */
  double *pdValue;
  pdValue = (double *)DBFReadAttribute(psDBF, iRecord, iField, 'c', iofailure, ismarked, offset);  /* changed from B to O 2.31.5 */
  if (pdValue == NULL)
    return 0.0;
  else
    return *pdValue;
}


double SHPAPI_CALL DBFReadIntegerAttribute (DBFHandle psDBF, int iRecord, int iField, char *iofailure, char *ismarked) {  /* 2.9.8 */
  double *pdValue;
  pdValue = (double *)DBFReadAttribute(psDBF, iRecord, iField, 'I', iofailure, ismarked, 0);
  /* see: http://www.dbase.com/Knowledgebase/dbulletin/bu12ce02.htm */
  if (*pdValue == 0.0 && signbit(*pdValue)) *pdValue = AGN_NAN;  /* 7.3.3 fix */
  if (pdValue == NULL)
    return 0.0;
  else
    return *pdValue;
}


double SHPAPI_CALL DBFReadUShortAttribute (DBFHandle psDBF, int iRecord, int iField, char *iofailure, char *ismarked) {  /* 7.3.3 */
  double *pdValue;
  pdValue = (double *)DBFReadAttribute(psDBF, iRecord, iField, 'u', iofailure, ismarked, 0);
  /* see: http://www.dbase.com/Knowledgebase/dbulletin/bu12ce02.htm */
  if (*pdValue == 0.0 && signbit(*pdValue)) *pdValue = AGN_NAN;  /* 7.3.3 fix */
  if (pdValue == NULL)
    return 0.0;
  else
    return *pdValue;
}


double SHPAPI_CALL DBFReadShortAttribute (DBFHandle psDBF, int iRecord, int iField, char *iofailure, char *ismarked) {  /* 7.3.3 */
  double *pdValue;
  pdValue = (double *)DBFReadAttribute(psDBF, iRecord, iField, 's', iofailure, ismarked, 0);
  /* see: http://www.dbase.com/Knowledgebase/dbulletin/bu12ce02.htm */
  if (*pdValue == 0.0 && signbit(*pdValue)) *pdValue = AGN_NAN;  /* 7.3.3 fix */
  if (pdValue == NULL)
    return 0.0;
  else
    return *pdValue;
}


double SHPAPI_CALL DBFReadDecimalAttribute (DBFHandle psDBF, int iRecord, int iField, char *iofailure, char *ismarked) {  /* 2.31.6 */
  double *pdValue;
  pdValue = (double *)DBFReadAttribute(psDBF, iRecord, iField, 'f', iofailure, ismarked, 0);
  /* see: http://www.dbase.com/Knowledgebase/dbulletin/bu12ce02.htm */
  if (pdValue == NULL)
    return 0.0;
  else
    return *pdValue;
}


double SHPAPI_CALL DBFReadByteAttribute (DBFHandle psDBF, int iRecord, int iField, char *iofailure, char *ismarked) {  /* 2.31.6 */
  double *pdValue;
  pdValue = (double *)DBFReadAttribute(psDBF, iRecord, iField, 'b', iofailure, ismarked, 0);
  if (pdValue == NULL)
    return 0.0;
  else
    return *pdValue;
}


double SHPAPI_CALL DBFReadTimeStampAttribute (DBFHandle psDBF, int iRecord, int iField, char *iofailure, char *ismarked, int *ms) {  /* 2.9.8 */
  double jd;
  if (!TimestampIsDouble) {
    uint64_t *val = (uint64_t *)DBFReadAttribute(psDBF, iRecord, iField, '@', iofailure, ismarked, 0);
    if (val == NULL) {
      *ms = 0;
      return 0.0;
    }
    /* first read integral Julian Date */
    jd = tools_uint64toint32(*val, 1);
    *ms = tools_uint64toint32(*val, 2);
  } else {
    double *val = (double *)DBFReadAttribute(psDBF, iRecord, iField, '@', iofailure, ismarked, 0);
    if (val == NULL) {
      *ms = 0;
      return 0.0;
    }
    *ms = luai_numfrac(*val - 0.5)*60L*60L*24L*1000L;
    jd = TRUNC(*val + 0.5);
  }
  return jd;
}


/************************************************************************/
/*                        DBFReadStringAttribute()                      */
/*                                                                      */
/*      Read a string attribute.                                        */
/************************************************************************/

const char SHPAPI_CALL1(*)
DBFReadStringAttribute (DBFHandle psDBF, int iRecord, int iField, char *iofailure, char *ismarked) {
  return((const char *) DBFReadAttribute(psDBF, iRecord, iField, 'C', iofailure, ismarked, 0));
}

/************************************************************************/
/*                        DBFReadStringAttribute()                      */
/*                                                                      */
/*      Read a string attribute.                                        */
/*                                                                      */
/* Agena 2.1.6                                                          */
/************************************************************************/

const char SHPAPI_CALL1(*) DBFReadDateAttribute (DBFHandle psDBF, int iRecord, int iField, char *iofailure, char *ismarked) {
  return((const char *) DBFReadAttribute(psDBF, iRecord, iField, 'D', iofailure, ismarked, 0));
}

/************************************************************************/
/*                        DBFReadLogicalAttribute()                     */
/*                                                                      */
/*      Read a logical attribute.                                       */
/************************************************************************/

const char SHPAPI_CALL1(*)
DBFReadLogicalAttribute (DBFHandle psDBF, int iRecord, int iField, char *iofailure, char *ismarked) {
  return((const char *)DBFReadAttribute(psDBF, iRecord, iField, 'L', iofailure, ismarked, 0));
}


#define pushnumericresult(L,iofailure,ismarked,fn,result) { \
  if (iofailure) \
    luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.readvalue"); \
  else if (ismarked) \
    lua_pushnil(L); \
  else \
    fn(L, result); \
}

static int xbase_readvalue (lua_State *L) {
  DBFHandle *hnd;
  int record, field, maxrecords, maxfields, fieldinfo;
  char iofailure, ismarked;
  record = agnL_checkinteger(L, 2);
  field = agnL_checkinteger(L, 3);
  hnd = aux_gethandle(L, 1, "xbase.readvalue");
  maxrecords = DBFGetRecordCount(*hnd);
  maxfields = DBFGetFieldCount(*hnd);
  if (maxrecords == 0 || maxfields == 0)
    luaL_error(L, "Error in " LUA_QS ": database is empty.", "xbase.readvalue");
  if (record < 1 || record > maxrecords)
    luaL_error(L, "Error in " LUA_QS ": record %d does not exist.", "xbase.readvalue", record);
  if (field < 1 || field > maxfields)
    luaL_error(L, "Error in " LUA_QS ": field %d does not exist.", "xbase.readvalue", field);
  field--; record--;
  fieldinfo = DBFGetFieldInfo(*hnd, field, NULL, NULL, NULL);
  switch (fieldinfo) {
    case FTString: case FTMemo: case FTOle: case FTBinary: {
      const char *result;
      result = DBFReadStringAttribute(*hnd, record, field, &iofailure, &ismarked);
      if (result == NULL)
        luaL_error(L, "Error in " LUA_QS ": no string stored in database.", "xbase.readvalue");
      else if (iofailure)
        luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.readvalue");
      else if (ismarked)
        lua_pushnil(L);
      else
        lua_pushstring(L, result);
      break;
    }
    case FTDouble: {
      lua_Number result;
      result = DBFReadDoubleAttribute(*hnd, record, field, &iofailure, &ismarked);  /* 2.2.0 RC 2 */
      pushnumericresult(L, iofailure, ismarked, lua_pushnumber, result);
      break;
    }
    case FTFloat: {
      lua_Number result;
      result = DBFReadFloatAttribute(*hnd, record, field, &iofailure, &ismarked);
      pushnumericresult(L, iofailure, ismarked, lua_pushnumber, result);
      break;
    }
    case FTBinDouble: {
      lua_Number result;
      result = DBFReadBinDoubleAttribute(*hnd, record, field, &iofailure, &ismarked);
      pushnumericresult(L, iofailure, ismarked, lua_pushnumber, result);
      break;
    }
    case FTInteger: {
      lua_Number result;
      result = DBFReadIntegerAttribute(*hnd, record, field, &iofailure, &ismarked);
      pushnumericresult(L, iofailure, ismarked, lua_pushnumber, result);
      break;
    }
    case FTUShort: {  /* 7.3.3 */
      lua_Number result;
      result = DBFReadUShortAttribute(*hnd, record, field, &iofailure, &ismarked);
      pushnumericresult(L, iofailure, ismarked, lua_pushnumber, result);
      break;
    }
    case FTShort: {  /* 7.3.3 */
      lua_Number result;
      result = DBFReadShortAttribute(*hnd, record, field, &iofailure, &ismarked);
      pushnumericresult(L, iofailure, ismarked, lua_pushnumber, result);
      break;
    }
    case FTBinComplex: {  /* 2.31.6 */
      lua_Number z[2];
      z[0] = DBFReadBinComplexAttribute(*hnd, record, field, &iofailure, &ismarked, 0);
      z[1] = DBFReadBinComplexAttribute(*hnd, record, field, &iofailure, &ismarked, sizeof(lua_Number));
      if (iofailure)
        luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.readvalue");
      else if (ismarked)
        lua_pushnil(L);
      else {
        agn_pushcomplex(L, z[0], z[1]);
      }
      break;
    }
    case FTByte: {  /* 2.31.6 */
      unsigned char result;
      result = DBFReadByteAttribute(*hnd, record, field, &iofailure, &ismarked);
      pushnumericresult(L, iofailure, ismarked, lua_pushinteger, result);
      break;
    }
    case FTDecimal: {  /* 2.31.6 */
      lua_Number result;
      result = DBFReadDecimalAttribute(*hnd, record, field, &iofailure, &ismarked);
      pushnumericresult(L, iofailure, ismarked, lua_pushnumber, result);
      break;
    }
    case FTDate: {
      const char *result;
      result = DBFReadStringAttribute(*hnd, record, field, &iofailure, &ismarked);
      if (result == NULL)
        luaL_error(L, "Error in " LUA_QS ": no date stored in database.", "xbase.readvalue");
      else if (iofailure)
        luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.readvalue");
      else if (ismarked)
        lua_pushnil(L);
      else
        lua_pushstring(L, result);
      break;
    }
    case FTTimeStamp: {
      lua_Number result;
      int ms;
      result = DBFReadTimeStampAttribute(*hnd, record, field, &iofailure, &ismarked, &ms);
      if (iofailure)
        luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.readvalue");
      else if (ismarked)
        lua_pushnil(L);
      else
        agn_createpairnumbers(L, result, ms);
      break;
    }
    case FTLogical: {
      const char *result;
      result = DBFReadLogicalAttribute(*hnd, record, field, &iofailure, &ismarked);
      if (result == NULL)
        luaL_error(L, "Error in " LUA_QS ": no logical value stored in database.", "xbase.readvalue");
      else if (iofailure)
        luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.readvalue");
      else if (ismarked)
        lua_pushnil(L);
      else {
        switch (*result) {
          case 'Y': case 'y': case 'T': case 't':
            lua_pushtrue(L);
            break;
          case 'N': case 'n': case 'F': case 'f':
            lua_pushfalse(L);
            break;
          default:
            lua_pushfail(L);
        }
      }
      break;
    }
    case FTInvalid: {  /* 2.31.3 */
      lua_pushfail(L);
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": unknown type of data.", "xbase.readvalue");
  }
  return 1;
}

/************************************************************************/
/*                         DBFIsAttributeNULL()                         */
/*                                                                      */
/*      Return TRUE if value for field is NULL.                         */
/*                                                                      */
/*      Contributed by Jim Matthews.                                    */
/************************************************************************/

int SHPAPI_CALL DBFIsAttributeNULL (DBFHandle psDBF, int iRecord, int iField) {
  const char *pszValue;
  char iofailure, ismarked;
  pszValue = DBFReadStringAttribute(psDBF, iRecord, iField, &iofailure, &ismarked);
  switch(psDBF->pachFieldType[iField]) {
    case 'N':
    case 'F':
    case 'B':  /* 2.2.0 RC 3 */
      /* NULL numeric fields have value "****************" */
      return pszValue[0] == '*';
    case 'D':
      /* NULL date fields have value "00000000" */
      return tools_strncmp(pszValue, "00000000", 8) == 0;
    case 'L':
      /* NULL boolean fields have value "?" */
      return pszValue[0] == '?';
    default:
      /* empty string fields are considered NULL */
      return tools_strlen(pszValue) == 0;  /* 2.17.8 tweak */
  }
}


static int xbase_isvoid (lua_State *L) {
  DBFHandle *hnd;
  int record, field, maxrecords, maxfields;
  record = agnL_checkinteger(L, 2);
  field = agnL_checkinteger(L, 3);
  hnd = aux_gethandle(L, 1, "xbase.isvoid");
  maxrecords = DBFGetRecordCount(*hnd);
  maxfields = DBFGetFieldCount(*hnd);
  if (maxrecords == 0 || maxfields == 0)
    luaL_error(L, "Error in " LUA_QS ": database is empty.", "xbase.isvoid");
  if (record < 1 || record > maxrecords)
    luaL_error(L, "Error in " LUA_QS ": record does not exist.", "xbase.isvoid");
  if (field < 1 || field > maxfields)
    luaL_error(L, "Error in " LUA_QS ": field does not exist.", "xbase.isvoid");
  lua_pushboolean(L, DBFIsAttributeNULL(*hnd, --record, --field));
  return 1;
}

/************************************************************************/
/*                          DBFGetFieldCount()                          */
/*                                                                      */
/*      Return the number of fields in this table.                      */
/************************************************************************/

int SHPAPI_CALL DBFGetFieldCount (DBFHandle psDBF) {
  return(psDBF->nFields);
}

/************************************************************************/
/*                       DBFGetNativeFieldType()                        */
/*                                                                      */
/*      Return the DBase field type for the specified field.            */
/*                                                                      */
/*      Value can be one of: 'C' (String), 'D' (Date), 'F' (Float),     */
/*                           'N' (Numeric, with or without decimal),    */
/*                           'L' (Logical),                             */
/*                           'M' (Memo: 10 digits .DBT block ptr)       */
/************************************************************************/

char SHPAPI_CALL DBFGetNativeFieldType (DBFHandle psDBF, int iField) {
  if (iField >=0 && iField < psDBF->nFields)
    return psDBF->pachFieldType[iField];
  return ' ';
}

/************************************************************************/
/*                         DBFGetRecordCount()                          */
/*                                                                      */
/*      Return the number of records in this table.                     */
/************************************************************************/

int SHPAPI_CALL DBFGetRecordCount (DBFHandle psDBF) {
  return(psDBF->nRecords);
}

/* assumes that table is on the top of the stack */

static void setintegerfield (lua_State *L, const char *key, int value) {
  lua_pushinteger(L, value);
  lua_setfield(L, -2, key);
}

/* assumes that table is on the top of the stack */

static void setstringfield (lua_State *L, const char *key, const char *value) {
  lua_pushstring(L, value);
  lua_setfield(L, -2, key);
}

/*
Code pages supported by dBASE III and Visual FoxPro

from: http://www.clicketyclick.dk/databases/xbase/format/dbf.html#DBF_NOTE_5_TARGET (dBASE III) and
      http://msdn.microsoft.com/en-us/library/8t45x02s%28v=vs.80%29.aspx  (Visual FoxPro)

(Foxpro) Code pages: These values follow the DOS / Windows Code Page values.
Value   Description   Code page
01h   DOS USA  code page 437
02h   DOS Multilingual (International MS-DOS)  code page 850
03h   Windows ANSI  code page 1252
04h   Standard Macintosh  code page 10000
64h   EE MS-DOS (Eastern European MS-DOS)  code page 852
65h   Nordic MS-DOS  code page 865
66h   Russian MS-DOS  code page 866
67h   Icelandic MS-DOS  code page 861
68h   Kamenicky (Czech) MS-DOS  code page 895
69h   Mazovia (Polish) MS-DOS   code page 620
6Ah   Greek MS-DOS (437G)
6Bh   Turkish MS-DOS  code page 857
78h     Traditional Chinese (Hong Kong SAR, Taiwan) Windows  code page 950
79h     Korean Windows  code page 949
7Ah     Chinese Simplified (PRC, Singapore) Windows  code page 936
7Bh     Japanese Windows  code page 932
7Ch     Thai Windows  code page 874
7Dh     Hebrew Windows  code page 1255
7Eh     Arabic Windows   code page 1256
96h   Russian Macintosh  code page 10007
97h   Eastern European Macintosh  code page 10029
98h   Greek Macintosh  code page 10006
C8h   Windows EE  (Eastern European Windows) code page 1250
C9h   Russian Windows  code page 1251
CAh   Turkish Windows  code page 1254
CBh   Greek Windows  code page 1253
*/

static int xbase_attrib (lua_State *L) {
  DBFHandle *hnd;
  DBFInfo dbf;
  int i, nfields;
  char szTitle[12];
  unsigned char s[2];
  hnd = aux_gethandle(L, 1, "xbase.attrib");
  dbf = **hnd;
  lua_createtable(L, 0, 9);
  setstringfield(L, "filename", dbf.filename);
  nfields = DBFGetFieldCount(*hnd);
  setintegerfield(L, "fields", nfields);
  setintegerfield(L, "records", DBFGetRecordCount(*hnd));
  setintegerfield(L, "headerlength", dbf.nHeaderLength);
  setintegerfield(L, "recordlength", dbf.nRecordLength);
  setintegerfield(L, "lastmodified", dbf.lastmodified);
  setstringfield(L, "codepage", dbf.pszCodePage);  /* changed 2.1.6 */
  setintegerfield(L, "languagedriver", dbf.iLanguageDriver);  /* changed 2.1.6 */
  setintegerfield(L, "version", dbf.dBaseVersion);  /* added 2.9.8 */
  switch (dbf.dBaseVersion) {  /* 2.31.6 */
    case 0x01: setstringfield(L, "versionname", "not used"); break;
    case 0x02: setstringfield(L, "versionname", "FoxBASE"); break;
    case 0x03: setstringfield(L, "versionname", "FoxBASE+/dBASE III PLUS, no memo"); break;
    case 0x04: setstringfield(L, "versionname", "dBASE Level 7"); break;
    case 0x07: setstringfield(L, "versionname", "agenaBASE Level 1"); break;
    case 0x05: setstringfield(L, "versionname", "dBASE 5, no memo"); break;
    case 0x30: setstringfield(L, "versionname", "Visual FoxPro"); break;
    case 0x31: setstringfield(L, "versionname", "Visual FoxPro, autoincrement enabled"); break;
    case 0x32: setstringfield(L, "versionname", "Visual FoxPro, Varchar, Varbinary, or Blob-enabled"); break;
    case 0x43: setstringfield(L, "versionname", "dBASE IV SQL table files, no memo"); break;
    case 0x63: setstringfield(L, "versionname", "dBASE IV SQL system files, no memo"); break;
    case 0x7B: setstringfield(L, "versionname", "dBASE IV, with memo"); break;
    case 0x83: setstringfield(L, "versionname", "FoxBASE+/dBASE III PLUS, with memo"); break;
    case 0x8B: setstringfield(L, "versionname", "dBASE IV, with memo"); break;
    case 0x8E: setstringfield(L, "versionname", "dBASE IV with SQL table"); break;
    case 0xCB: setstringfield(L, "versionname", "dBASE IV SQL table files, with memo"); break;
    case 0xE5: setstringfield(L, "versionname", "Clipper SIX driver, with SMT memo"); break;
    case 0xF5: setstringfield(L, "versionname", "FoxPro 2.x (or earlier) with memo"); break;
    case 0xFB: setstringfield(L, "versionname", "FoxBASE (with memo?)"); break;
    default:
      setstringfield(L, "versionname", "unknown");
  }
  lua_pushstring(L, "fieldinfo");
  lua_newtable(L);
  lua_settable(L, -3);
  for (i=0; i < nfields; i++) {
    DBFFieldType eType;
    const char *pszTypeName = NULL;
    int nWidth = 0, nDecimals = 0;
    eType = DBFGetFieldInfo(*hnd, i, szTitle, &nWidth, &nDecimals);
    if (eType == FTString || eType == FTDate)  /* 2.1.7 fix */
      pszTypeName = "string";
    else if (eType == FTDouble  || eType == FTFloat     || eType == FTBinDouble || eType == FTBinComplex ||
             eType == FTInteger || eType == FTTimeStamp || eType == FTByte      || eType == FTDecimal ||
             eType == FTUShort  || eType == FTShort)  /* 2.1.7 fix / 2.2.0 RC 3 / 2.9.8 */
      pszTypeName = "number";
    else if (eType == FTLogical)
      pszTypeName = "boolean";
    else if (eType == FTInvalid)
      pszTypeName = "(invalid)";
    lua_getfield(L, -1, "fieldinfo");
    lua_pushinteger(L, i + 1);
    lua_createtable(L, 0, 4);
    setstringfield(L, "type", pszTypeName);
    s[0] = DBFGetNativeFieldType(*hnd, i);
    s[1] = '\0';  /* better sure than sorry */
    setstringfield(L, "nativetype", (const char *)s);
    setstringfield(L, "title", szTitle);
    setintegerfield(L, "width", nWidth);
    setintegerfield(L, "scale", nDecimals);
    lua_settable(L, -3);
    agn_poptop(L);
  }
  return 1;
}


static int xbase_header (lua_State *L) {  /* 2.2.0 */
  DBFHandle *hnd;
  DBFInfo dbf;
  int i, nfields;
  char szTitle[12];
  unsigned char s[2];
  hnd = aux_gethandle(L, 1, "xbase.attrib");
  dbf = **hnd;
  (void)dbf;  /* to avoid compiler warnings */
  nfields = DBFGetFieldCount(*hnd);
  luaL_checkstack(L, 3, "not enough stack space");  /* 3.5.5 */
  agn_createseq(L, nfields);  /* field names */
  agn_createseq(L, nfields);  /* Agena types */
  agn_createseq(L, nfields);  /* dBASE types */
  for (i=0; i < nfields; i++) {
    DBFFieldType eType;
    const char *pszTypeName = NULL;
    int nWidth = 0, nDecimals = 0;
    eType = DBFGetFieldInfo(*hnd, i, szTitle, &nWidth, &nDecimals);
    if (eType == FTString || eType == FTDate)
      pszTypeName = "string";
    else if (eType == FTDouble  || eType == FTFloat     || eType == FTBinDouble || eType == FTBinComplex ||
             eType == FTInteger || eType == FTTimeStamp || eType == FTByte      || eType == FTDecimal ||
             eType == FTUShort  || eType == FTShort)  /* 2.9.8 */
      pszTypeName = "number";
    else if (eType == FTLogical)
      pszTypeName = "boolean";
    else if (eType == FTInvalid)
      pszTypeName = "(invalid)";
    lua_seqrawsetistring(L, -3, i + 1, szTitle);
    lua_seqrawsetistring(L, -2, i + 1, pszTypeName);
    s[0] = DBFGetNativeFieldType(*hnd, i);
    s[1] = '\0';  /* better sure than sorry */
    lua_seqrawsetistring(L, -1, i + 1, (const char *)s);
  }
  return 3;
}


/************************************************************************/
/*                          DBFGetFieldInfo()                           */
/*                                                                      */
/*      Return any requested information about the field.               */
/************************************************************************/

DBFFieldType SHPAPI_CALL
DBFGetFieldInfo (DBFHandle psDBF, int iField, char * pszFieldName,
                 int * pnWidth, int * pnDecimals) {
  if (iField < 0 || iField >= psDBF->nFields) return(FTInvalid);
  if (pnWidth != NULL) *pnWidth = psDBF->panFieldSize[iField];
  if (pnDecimals != NULL) *pnDecimals = psDBF->panFieldDecimals[iField];
  if (pszFieldName != NULL) {
    int i;
    strncpy(pszFieldName, (char *) psDBF->pszHeader + iField*32, 11);
    pszFieldName[11] = '\0';
    for (i=10; i > 0 && pszFieldName[i] == ' '; i--) pszFieldName[i] = '\0';
  }
  switch (psDBF->pachFieldType[iField]) {
    case 'L': return FTLogical;
    case 'D': return FTDate;        /* 2.1.6 */
    case 'N': return FTDouble;
    case 'F': return FTFloat;
    case 'B': return FTBinary;      /* 2.2.0 RC 3, changed from FTBinDouble 2.31.3 */
    case 'O': return FTBinDouble;   /* 2.2.0 RC 3, changed from 'B' to 'O' in 2.31.3 */
    case 'c': return FTBinComplex;  /* 2.31.6 */
    case 'I': return FTInteger;     /* 2.9.8 */
    case 'u': return FTUShort;      /* 7.3.3 */
    case 's': return FTShort;       /* 7.3.3 */
    case '@': return FTTimeStamp;   /* 2.9.8, 2.31.3 changed from T to @ */
    case 'C': return FTString;      /* 2.31.3 */
    case 'M': return FTMemo;        /* 2.31.3 */
    case 'G': return FTOle;         /* 2.31.3 */
    case 'b': return FTByte;        /* 2.31.6 */
    case 'f': return FTDecimal;     /* 2.31.6 */
    default:
      return FTInvalid;
  }
}

/************************************************************************/
/*                         DBFWriteAttribute()                          */
/*                                                                      */
/*  Write an attribute record to the file.                              */
/************************************************************************/

#if BYTE_ORDER == BIG_ENDIAN
#define aux_writetodouble(var,pv) { \
  var = (DoubleIsBigEndian) ? \
        tools_doubletouint64(pv) : \
        tools_doubletouint64andswap(pv); \
}
#else
#define aux_writetodouble(var,pv) { \
  var = (DoubleIsBigEndian) ? \
        tools_doubletouint64andswap(pv) : \
        tools_doubletouint64(pv); \
}
#endif

static int DBFWriteAttribute (DBFHandle psDBF, int hEntity, int iField, void *pValue, int offset) {
  int nRecordOffset, i, j, nRetResult = TRUE;
  int isimaginary = psDBF->pachFieldType[iField] == 'c' && offset != 0;
  unsigned char *pabyRec;
  char szSField[400], szFormat[20];
  /* Is this a valid record ? */
  if (hEntity < 0 || hEntity > psDBF->nRecords || hEntity == INT_MAX)  /* INT_MAX check added 0.32.4 */
    return FALSE;
  if (psDBF->bNoHeader) DBFWriteHeader(psDBF);
  /* Is this a brand new record ? */
  if (hEntity == psDBF->nRecords) {
    DBFFlushRecord(psDBF);
    psDBF->nRecords++;
    for (i=0; i < psDBF->nRecordLength; i++)
      psDBF->pszCurrentRecord[i] = ' ';
    psDBF->nCurrentRecord = hEntity;
  }
  /* Is this an existing record, but different than the last one we accessed ? */
  if (psDBF->nCurrentRecord != hEntity) {
    int r;
    DBFFlushRecord(psDBF);
    nRecordOffset = psDBF->nRecordLength * hEntity + psDBF->nHeaderLength;
    _fseeki64(psDBF->fp, nRecordOffset, 0);
    r = fread(psDBF->pszCurrentRecord, psDBF->nRecordLength, 1, psDBF->fp);
    (void)r;  /* prevent compiler warning */
    psDBF->nCurrentRecord = hEntity;
  }
  pabyRec = (unsigned char *)psDBF->pszCurrentRecord;
  psDBF->bCurrentRecordModified = TRUE;
  psDBF->bUpdated = TRUE;
  /* Translate NULL value to valid DBF file representation. Contributed by Jim Matthews. */
  if (pValue == NULL) {
    switch(psDBF->pachFieldType[iField]) {
      case 'N':
      case 'F':
      case 'B':  /* 2.2.0 RC 3 */
        /* NULL numeric fields have value "****************" */
        if (!isimaginary) memset((char *)(pabyRec + psDBF->panFieldOffset[iField]), '*',
                    psDBF->panFieldSize[iField]);
        break;
      case 'D':
        /* NULL date fields have value "00000000" */
        memset((char *)(pabyRec + psDBF->panFieldOffset[iField]), '0',
                    psDBF->panFieldSize[iField]);
        break;
      case 'L':
        /* NULL boolean fields have value "?" */
        memset((char *)(pabyRec + psDBF->panFieldOffset[iField]), '?',
                    psDBF->panFieldSize[iField]);
        break;
      default:
        /* all other empty fields are considered NULL */
        memset((char *)(pabyRec + psDBF->panFieldOffset[iField]), '\0',
                    psDBF->panFieldSize[iField]);
        break;
    }
    return TRUE;
  }
  /* Assign all the record fields. */
  switch (psDBF->pachFieldType[iField]) {
    case 'N':
    case 'F':
      if (psDBF->panFieldDecimals[iField] == 0) {  /* integer ? */
        int  nWidth = psDBF->panFieldSize[iField];
        if (sizeof(szSField) - 2 < nWidth)
          nWidth = sizeof(szSField) - 2;
        sprintf(szFormat, "%%%dd", nWidth);
        sprintf(szSField, szFormat, (int)*((double *)pValue));
        if ((int)tools_strlen(szSField) > psDBF->panFieldSize[iField]) {  /* 2.17.8 tweak */
          szSField[psDBF->panFieldSize[iField]] = '\0';
          nRetResult = FALSE;
        }
        strcpy((char *)(pabyRec + psDBF->panFieldOffset[iField]), szSField);  /* 3.4.4 patch */
      } else {
        int nWidth = psDBF->panFieldSize[iField];
        if (sizeof(szSField) - 2 < nWidth)
          nWidth = sizeof(szSField) - 2;
        sprintf(szFormat, "%%%d.%df",
          nWidth, psDBF->panFieldDecimals[iField]);
        sprintf(szSField, szFormat, *((double *)pValue));
        if ((int)tools_strlen(szSField) > psDBF->panFieldSize[iField]) {  /* 2.17.8 tweak */
          szSField[psDBF->panFieldSize[iField]] = '\0';
          nRetResult = FALSE;
        }
        strcpy((char *)(pabyRec+psDBF->panFieldOffset[iField]), szSField);  /* 3.4.4 patch */
      }
      break;
    case 'O': { /* binary double, 2.2.0 RC 3, changed from B to O in 2.31.4 fix */
      uint64_t ulong;
      aux_writetodouble(ulong, *((double *)(pValue)));
      tools_memcpy(pabyRec + psDBF->panFieldOffset[iField], &ulong, sizeof(uint64_t));  /* 2.21.5 tweak */
      break;
    }
    case 'c': { /* binary double complex, 2.31.5 */
      uint64_t ulong;
      lua_Number x = *((double *)(pValue));
#if BYTE_ORDER == BIG_ENDIAN
      ulong = tools_doubletouint64andswap(x);
#else
      ulong = tools_doubletouint64(x);
#endif
      tools_memcpy(pabyRec+psDBF->panFieldOffset[iField] + offset, &ulong, sizeof(uint64_t));
      break;
    }
    case 'I': { /* Long, signed binary 4-byte integer, 2.9.8 */
      int32_t slong;
#if BYTE_ORDER == BIG_ENDIAN
      slong = (LongIsBigEndian) ?
              *((double *)pValue) :
              tools_swapint32(*((double *)pValue));
#else
      slong = (LongIsBigEndian) ?
              tools_swapint32(*((double *)pValue)) :
              *((double *)pValue);
#endif
      tools_memcpy(pabyRec+psDBF->panFieldOffset[iField], &slong, sizeof(int32_t));  /* 2.21.5 tweak */
      break;
    }
    case 'u': { /* Unsigned Short, unsigned binary 2-byte integer, 7.3.3 */
      uint16_t sshort;
#if BYTE_ORDER == BIG_ENDIAN
      sshort = (LongIsBigEndian) ?
              *((double *)pValue) :
              tools_swapuint16(*((double *)pValue));
#else
      sshort = (LongIsBigEndian) ?
              tools_swapuint16(*((double *)pValue)) :
              *((double *)pValue);
#endif
      tools_memcpy(pabyRec+psDBF->panFieldOffset[iField], &sshort, sizeof(uint16_t));
      break;
    }
    case 's': { /* Signed Short, signed binary 2-byte integer, 7.3.3 */
      int16_t sshort;
#if BYTE_ORDER == BIG_ENDIAN
      sshort = (LongIsBigEndian) ?
              *((double *)pValue) :
              tools_swapuint16(*((double *)pValue));
#else
      sshort = (LongIsBigEndian) ?
              tools_swapuint16(*((double *)pValue)) :
              *((double *)pValue);
#endif
      tools_memcpy(pabyRec+psDBF->panFieldOffset[iField], &sshort, sizeof(int16_t));
      break;
    }
    case 'f': { /* 4-byte float, 2.31.6 */
      uint32_t ulong;
#if BYTE_ORDER == BIG_ENDIAN
      ulong = tools_swapuint32((float)*((double *)pValue));
#else
      ulong = tools_doubletouint32((float)*((double *)pValue));
#endif
      tools_memcpy(pabyRec+psDBF->panFieldOffset[iField], &ulong, sizeof(uint32_t));
      break;
    }
    case 'b': { /* unsigned char, 2.31.6 */
      unsigned char slong = *((double *)pValue);
      tools_memcpy(pabyRec+psDBF->panFieldOffset[iField], &slong, sizeof(unsigned char));
      break;
    }
    case '@': { /* Timestamp, two Longs, binary 4-byte integers OR a Big or Little Endian double, 2.9.8 */
      if (!TimestampIsDouble) {
        uint64_t slong;
        slong = (*((uint64_t *)pValue));
        tools_memcpy(pabyRec+psDBF->panFieldOffset[iField], &slong, sizeof(uint64_t));
      } else {
        double slong;
        slong = (*((double *)pValue));
        tools_memcpy(pabyRec+psDBF->panFieldOffset[iField], &slong, sizeof(double));
      }
      break;
    }
    case 'L':  /* Logical */
      if (psDBF->panFieldSize[iField] >= 1 &&
        (*(char*)pValue == 'F' || *(char*)pValue == 'T'))  /* 2.2.0 RC 3 */
         *(pabyRec+psDBF->panFieldOffset[iField]) = *(char*)pValue;
      break;
    default:  /* Characters, Dates */
      if ((int) tools_strlen((char *)pValue) > psDBF->panFieldSize[iField]) {  /* 2.17.8 tweak */
        j = psDBF->panFieldSize[iField];
        nRetResult = FALSE;
      } else {
        memset(pabyRec+psDBF->panFieldOffset[iField], ' ',
                    psDBF->panFieldSize[iField]);
        j = tools_strlen((char *)pValue);  /* 2.17.8 tweak */
      }
      strncpy((char *)(pabyRec+psDBF->panFieldOffset[iField]), (char *)pValue, j);
      break;
  }
  return nRetResult;
}


/* helper function for write functions */
void aux_checkpositions (lua_State *L, DBFHandle hnd, int *record, int *field, int checkempty, const char *procname) {
  int actrecords, actfields;
  actrecords = DBFGetRecordCount(hnd);
  actfields = DBFGetFieldCount(hnd);
  *record = agnL_checkinteger(L, 2) - 1;
  if (actfields == 0 || (checkempty && actrecords == 0))
    luaL_error(L, "Error in " LUA_QS ": database is empty.", procname);
  if (*record < 0 || *record > actrecords)
    luaL_error(L, "Error in " LUA_QS ": invalid record given, must be in [1, %d].", procname, actrecords + 1);
  *field = agnL_checkinteger(L, 3) - 1;
  if (*field < 0 || *field >= actfields)
    luaL_error(L, "Error in " LUA_QS ": invalid field given, must be in [1, %d].", procname, actfields);
}

/************************************************************************/
/*                      DBFWriteNumberAttribute()                       */
/*                                                                      */
/*  Write a C double as either dBASE float, numeric, or binary integer  */
/************************************************************************/

int SHPAPI_CALL DBFWriteNumberAttribute (DBFHandle psDBF, int iRecord, int iField, double dValue, int offset) {
  return DBFWriteAttribute(psDBF, iRecord, iField, (void *) &dValue, offset);
}


static int xbase_writenumber (lua_State *L) {
  DBFHandle *hnd;
  DBFFieldType dBASEtype;
  int record, field;
  hnd = aux_gethandle(L, 1, "xbase.writenumber");
  if (!tools_writable((*hnd)->fp))  /* 7.3.3 extension */
    luaL_error(L, "Error in " LUA_QS ": file is in read mode, pass the 'write' option to `xbase.open`.", "xbase.writenumber");
  aux_checkpositions(L, *hnd, &record, &field, 0, "xbase.writenumber");
  dBASEtype = DBFGetFieldInfo(*hnd, field, NULL, NULL, NULL);
  if (dBASEtype == FTDouble  || dBASEtype == FTFloat || dBASEtype == FTBinDouble ||
      dBASEtype == FTInteger || dBASEtype == FTByte  || dBASEtype == FTDecimal ||
      dBASEtype == FTUShort  || dBASEtype == FTShort) { /* 2.9.8 improvements */
    lua_Number val = agn_checknumber(L, 4);
    lua_pushboolean(L, DBFWriteNumberAttribute(*hnd, record, field, val, 0));
  } else if (dBASEtype == FTBinComplex) {  /* 2.31.6 */
    lua_Number re, im;
    if (!agn_getcmplxparts(L, 4, &re, &im))
      luaL_error(L, "Error in " LUA_QS ": expected a complex number, got %s.", "xbase.writenumber", luaL_typename(L, 4));
    lua_pushboolean(L,
      DBFWriteNumberAttribute(*hnd, record, field, re, 0) == TRUE &&
      DBFWriteNumberAttribute(*hnd, record, field, im, sizeof(lua_Number) == TRUE)
    );
  } else
    luaL_error(L, "Error in " LUA_QS ": field is not of type float, short, long or (complex/binary) double.", "xbase.writenumber");
  return 1;
}


static int xbase_writefloat (lua_State *L) {
  DBFHandle *hnd;
  int record, field;
  lua_Number val;
  hnd = aux_gethandle(L, 1, "xbase.writefloat");
  if (!tools_writable((*hnd)->fp))  /* 7.3.3 extension */
    luaL_error(L, "Error in " LUA_QS ": file is in read mode, pass the 'write' option to `xbase.open`.", "xbase.writefloat");
  aux_checkpositions(L, *hnd, &record, &field, 0, "xbase.writefloat");
  val = agn_checknumber(L, 4);
  if (DBFGetFieldInfo(*hnd, field, NULL, NULL, NULL) == FTFloat)
    lua_pushboolean(L, DBFWriteNumberAttribute(*hnd, record, field, val, 0));
  else
    luaL_error(L, "Error in " LUA_QS ": field is not of type float.", "xbase.writefloat");
  return 1;
}


static int xbase_writedouble (lua_State *L) {  /* 2.2.0 RC 3 */
  DBFHandle *hnd;
  int record, field;
  lua_Number val;
  hnd = aux_gethandle(L, 1, "xbase.writedouble");
  if (!tools_writable((*hnd)->fp))  /* 7.3.3 extension */
    luaL_error(L, "Error in " LUA_QS ": file is in read mode, pass the 'write' option to `xbase.open`.", "xbase.writedouble");
  aux_checkpositions(L, *hnd, &record, &field, 0, "xbase.writedouble");
  val = agn_checknumber(L, 4);
  if (DBFGetFieldInfo(*hnd, field, NULL, NULL, NULL) == FTBinDouble)
    lua_pushboolean(L, DBFWriteNumberAttribute(*hnd, record, field, val, 0));
  else
    luaL_error(L, "Error in " LUA_QS ": field is not of type binary double.", "xbase.writedouble");  /* 2.9.8 */
  return 1;
}


static int xbase_writelong (lua_State *L) {  /* 2.9.8 */
  DBFHandle *hnd;
  int record, field;
  lua_Number val;
  hnd = aux_gethandle(L, 1, "xbase.writelong");
  if (!tools_writable((*hnd)->fp))  /* 7.3.3 extension */
    luaL_error(L, "Error in " LUA_QS ": file is in read mode, pass the 'write' option to `xbase.open`.", "xbase.writelong");
  aux_checkpositions(L, *hnd, &record, &field, 0, "xbase.writelong");
  val = agn_checknumber(L, 4);
  if (val < INT32_MIN || val > INT32_MAX)  /* 7.3.3 fix */
    luaL_error(L, "Error in " LUA_QS ": number %d too small or too big.", "xbase.writelong", (int32_t)val);  /* 7.3.3 fix */
  /* see: http://www.dbase.com/Knowledgebase/dbulletin/bu12ce02.htm */
  if (tools_isnan(val)) val = -0;  /* 2.10.0 fix */
  else if (val == 0)  val = +0;
  if (DBFGetFieldInfo(*hnd, field, NULL, NULL, NULL) == FTInteger)
    lua_pushboolean(L, DBFWriteNumberAttribute(*hnd, record, field, (int32_t)val, 0));
  else
    luaL_error(L, "Error in " LUA_QS ": field is not of type int32_t.", "xbase.writelong");
  return 1;
}


static int xbase_writeushort (lua_State *L) {  /* 7.3.3 */
  DBFHandle *hnd;
  int record, field;
  lua_Number val;
  hnd = aux_gethandle(L, 1, "xbase.writeushort");
  if (!tools_writable((*hnd)->fp))  /* 7.3.3 extension */
    luaL_error(L, "Error in " LUA_QS ": file is in read mode, pass the 'write' option to `xbase.open`.", "xbase.writeushort");
  aux_checkpositions(L, *hnd, &record, &field, 0, "xbase.writeushort");
  val = agn_checknumber(L, 4);
  if (val < 0.0 || val > UINT16_MAX)
    luaL_error(L, "Error in " LUA_QS ": number %d too small or too big.", "xbase.writeushort", (uint16_t)val);
  /* see: http://www.dbase.com/Knowledgebase/dbulletin/bu12ce02.htm */
  if (tools_isnan(val) || (val == 0 && signbit(val))) val = +0;
  if (DBFGetFieldInfo(*hnd, field, NULL, NULL, NULL) == FTUShort)
    lua_pushboolean(L, DBFWriteNumberAttribute(*hnd, record, field, (uint16_t)val, 0));
  else
    luaL_error(L, "Error in " LUA_QS ": field is not of type uint16_t.", "xbase.writeushort");
  return 1;
}


static int xbase_writeshort (lua_State *L) {  /* 7.3.3 */
  DBFHandle *hnd;
  int record, field;
  lua_Number val;
  hnd = aux_gethandle(L, 1, "xbase.writeshort");
  if (!tools_writable((*hnd)->fp))  /* 7.3.3 extension */
    luaL_error(L, "Error in " LUA_QS ": file is in read mode, pass the 'write' option to `xbase.open`.", "xbase.writeshort");
  aux_checkpositions(L, *hnd, &record, &field, 0, "xbase.writeshort");
  val = agn_checknumber(L, 4);
  if (val < INT16_MIN || val > INT16_MAX)
    luaL_error(L, "Error in " LUA_QS ": number %d too small or too big.", "xbase.writeshort", (int16_t)val);
  /* see: http://www.dbase.com/Knowledgebase/dbulletin/bu12ce02.htm */
  if (tools_isnan(val)) val = -0;
  else if (val == 0)  val = +0;
  if (DBFGetFieldInfo(*hnd, field, NULL, NULL, NULL) == FTShort)
    lua_pushboolean(L, DBFWriteNumberAttribute(*hnd, record, field, (int16_t)val, 0));
  else
    luaL_error(L, "Error in " LUA_QS ": field is not of type int16_t.", "xbase.writeshort");
  return 1;
}


static int xbase_writedecimal (lua_State *L) {  /* 2.31.6 */
  DBFHandle *hnd;
  int record, field;
  lua_Number val;
  hnd = aux_gethandle(L, 1, "xbase.writedecimal");
  if (!tools_writable((*hnd)->fp))  /* 7.3.3 extension */
    luaL_error(L, "Error in " LUA_QS ": file is in read mode, pass the 'write' option to `xbase.open`.", "xbase.writedecimal");
  aux_checkpositions(L, *hnd, &record, &field, 0, "xbase.writedecimal");
  val = agn_checknumber(L, 4);
  if (val < FLT_MIN || val > FLT_MAX)
    luaL_error(L, "Error in " LUA_QS ": number %d too small or too big.", "xbase.writelong", (int)val);
  /* see: http://www.dbase.com/Knowledgebase/dbulletin/bu12ce02.htm */
  if (DBFGetFieldInfo(*hnd, field, NULL, NULL, NULL) == FTDecimal)
    lua_pushboolean(L, DBFWriteNumberAttribute(*hnd, record, field, (float)val, 0));
  else
    luaL_error(L, "Error in " LUA_QS ": field is not of type decimal.", "xbase.writelong");
  return 1;
}


static int xbase_writecomplex (lua_State *L) {  /* 2.31.6 */
  DBFHandle *hnd;
  int record, field;
  lua_Number re, im;
  hnd = aux_gethandle(L, 1, "xbase.writecomplex");
  if (!tools_writable((*hnd)->fp))  /* 7.3.3 extension */
    luaL_error(L, "Error in " LUA_QS ": file is in read mode, pass the 'write' option to `xbase.open`.", "xbase.writecomplex");
  aux_checkpositions(L, *hnd, &record, &field, 0, "xbase.writecomplex");
  if (!agn_getcmplxparts(L, 4, &re, &im))
    luaL_error(L, "Error in " LUA_QS ": expected a complex number, got %s.", "xbase.writecomplex", luaL_typename(L, 4));
  if (DBFGetFieldInfo(*hnd, field, NULL, NULL, NULL) == FTBinComplex) {
    int a, b;
    a = DBFWriteNumberAttribute(*hnd, record, field, re, 0) == TRUE;
    b = DBFWriteNumberAttribute(*hnd, record, field, im, sizeof(lua_Number)) == TRUE;
    lua_pushboolean(L, a && b);
  } else
    luaL_error(L, "Error in " LUA_QS ": field is not of type binary complex.", "xbase.writecomplex");  /* 2.9.8 */
  return 1;
}


static int xbase_writebyte (lua_State *L) {  /* 2.31.6 */
  DBFHandle *hnd;
  int record, field;
  lua_Number val;
  hnd = aux_gethandle(L, 1, "xbase.writebyte");
  if (!tools_writable((*hnd)->fp))  /* 7.3.3 extension */
    luaL_error(L, "Error in " LUA_QS ": file is in read mode, pass the 'write' option to `xbase.open`.", "xbase.writebyte");
  aux_checkpositions(L, *hnd, &record, &field, 0, "xbase.writebyte");
  val = agn_checknumber(L, 4);
  if (val < 0 || val > 255 || tools_isfrac(val))
    luaL_error(L, "Error in " LUA_QS ": number too small, too big or not integral.", "xbase.writebyte");
  /* see: http://www.dbase.com/Knowledgebase/dbulletin/bu12ce02.htm */
  if (tools_isnan(val) || (val == 0 && signbit(val))) val = +0;  /* 7.3.3 fix */
  if (DBFGetFieldInfo(*hnd, field, NULL, NULL, NULL) == FTByte)
    lua_pushboolean(L, DBFWriteNumberAttribute(*hnd, record, field, val, 0));
  else
    luaL_error(L, "Error in " LUA_QS ": field is not of type byte.", "xbase.writebyte");
  return 1;
}


/************************************************************************/
/*                      DBFWriteStringAttribute()                       */
/*                                                                      */
/*      Write a string attribute.                                       */
/************************************************************************/

int SHPAPI_CALL DBFWriteStringAttribute (DBFHandle psDBF, int iRecord, int iField,
    const char *pszValue) {
  return DBFWriteAttribute(psDBF, iRecord, iField, (void *)pszValue, 0);
}


static int xbase_writestring (lua_State *L) {
  DBFHandle *hnd;
  int record, field;
  const char *val;
  hnd = aux_gethandle(L, 1, "xbase.writestring");
  aux_checkpositions(L, *hnd, &record, &field, 0, "xbase.writestring");
  val = luaL_checkstring(L, 4);
  if (DBFGetFieldInfo(*hnd, field, NULL, NULL, NULL) == FTString)
    lua_pushboolean(L, DBFWriteStringAttribute(*hnd, record, field, val));
  else
    luaL_error(L, "Error in " LUA_QS ": field is not of type character.", "xbase.writestring");
  return 1;
}


static int xbase_writedate (lua_State *L) {
  DBFHandle *hnd;
  int record, field, dateval;
  const char *val;
  hnd = aux_gethandle(L, 1, "xbase.writedate");
  aux_checkpositions(L, *hnd, &record, &field, 0, "xbase.writedate");
  val = luaL_checkstring(L, 4);  /* 2.8.0 patch */
  dateval = atoi(val);
  if (dateval < 19000101 || dateval > 99991231)
    luaL_error(L, "Error in " LUA_QS ": date must be an integer in [19000101, 99991231].", "xbase.writedate");
  else if (tools_strlen(val) != 8)  /* 2.17.8 tweak */
    luaL_error(L, "Error in " LUA_QS ": date of the format `YYYYMMDD` expected.", "xbase.writedate");
  if (DBFGetFieldInfo(*hnd, field, NULL, NULL, NULL) == FTDate)
    lua_pushboolean(L, DBFWriteStringAttribute(*hnd, record, field, val));
  else
    luaL_error(L, "Error in " LUA_QS ": field is not of type date.", "xbase.writedate");
  return 1;
}


/************************************************************************/
/*                     DBFWriteTimeStampAttribute()                     */
/*                                                                      */
/*      Write a timestamp attribute.                                    */
/************************************************************************/

int SHPAPI_CALL DBFWriteTimeStampAttribute (DBFHandle psDBF, int iRecord, int iField, uint64_t dValue) {
  return DBFWriteAttribute(psDBF, iRecord, iField, (void *) &dValue, 0);
}


int SHPAPI_CALL DBFWriteTimeStampAttributeAsDouble (DBFHandle psDBF, int iRecord, int iField, double dValue) {
  return DBFWriteAttribute(psDBF, iRecord, iField, (void *) &dValue, 0);
}


/* see: http://www.dbase.com/KnowledgeBase/int/db7_file_fmt.htm
   "8 bytes - two longs, first for date, second for time. The date is the number of days since 01/01/4713 BC [i.e. Julian Date].
   Time is hours * 3600000L + minutes * 60000L + Seconds * 1000L", 2.9.8, fixed 7.8.4 */
static int xbase_writetime (lua_State *L) {
  DBFHandle *hnd;
  int record, field;
  int32_t yy, mm, dd, h, m, s, ms;
  lua_Number jd, intjd, totalms; // Added totalms to prevent overwriting 'ms'
  hnd = aux_gethandle(L, 1, "xbase.writetime");
  aux_checkpositions(L, *hnd, &record, &field, 0, "xbase.writetime");
  yy = agn_checknumber(L, 4);
  mm = agn_checknumber(L, 5);
  dd = agn_checknumber(L, 6);
  h = luaL_optint(L, 7, 0);
  m = luaL_optint(L, 8, 0);
  s = luaL_optint(L, 9, 0);
  ms = luaL_optint(L, 10, 0);  /* milliseconds */
  if (ms < 0 || ms > 999)
    luaL_error(L, "Error in " LUA_QS ": milliseconds must be in the range 0 .. 999.", "xbase.writetime");
  jd = iauJuliandate(yy, mm, dd, 0, 0, 0);
  if (jd == HUGE_VAL)
    luaL_error(L, "Error in " LUA_QS ": invalid date given.", "xbase.writetime");
  intjd = TRUNC(jd);
  totalms = (lua_Number)h*3600000.0 + (lua_Number)m*60000.0 + (lua_Number)s*1000.0 + (lua_Number)ms;
  jd = jd + totalms/86400000.0;
  if (DBFGetFieldInfo(*hnd, field, NULL, NULL, NULL) == FTTimeStamp) {
    if (!TimestampIsDouble) {
      /* dBASE 7 standard binary layout: 4 bytes of intjd, 4 bytes of totalms */
      uint64_t val = tools_twoint32touint64((int32_t)intjd, (int32_t)totalms);
      lua_pushboolean(L,
        DBFWriteTimeStampAttribute(*hnd, record, field, val));
    } else {
      lua_pushboolean(L,
        DBFWriteTimeStampAttributeAsDouble(*hnd, record, field, jd));
    }
  } else {
    luaL_error(L, "Error in " LUA_QS ": field is not of type timestamp.", "xbase.writetime");
  }
  return 1;
}


/************************************************************************/
/*                      DBFWriteNULLAttribute()                         */
/*                                                                      */
/*      Overwrite an attribute.                                         */
/************************************************************************/

static int xbase_purge (lua_State *L) {
  DBFHandle *hnd;
  int record, field;
  hnd = aux_gethandle(L, 1, "xbase.purge");
  aux_checkpositions(L, *hnd, &record, &field, 1, "xbase.purge");
  lua_pushboolean(L, DBFWriteAttribute(*hnd, record, field, NULL, 0));
  return 1;
}


/************************************************************************/
/*                      DBFWriteLogicalAttribute()                      */
/*                                                                      */
/*      Write a logical attribute.                                      */
/************************************************************************/

int SHPAPI_CALL DBFWriteLogicalAttribute (DBFHandle psDBF, int iRecord, int iField, const char lValue) {
  return DBFWriteAttribute(psDBF, iRecord, iField, (void *)(&lValue), 0);
}


static int xbase_writeboolean (lua_State *L) {
  DBFHandle *hnd;
  int record, field, val;
  hnd = aux_gethandle(L, 1, "xbase.writeboolean");
  aux_checkpositions(L, *hnd, &record, &field, 0, "xbase.writeboolean");
  val = (lua_isboolean(L, 4)) ? lua_toboolean(L, 4) : -1;
  if (DBFGetFieldInfo(*hnd, field, NULL, NULL, NULL) == FTLogical) {
    switch (val) {
      case 0:
        lua_pushboolean(L, DBFWriteLogicalAttribute(*hnd, record, field, 'F'));
        break;
      case 1:
        lua_pushboolean(L, DBFWriteLogicalAttribute(*hnd, record, field, 'T'));
        break;
      default:
        luaL_error(L, "Error in " LUA_QS ": true or false as fourth argument expected.", "xbase.writeboolean");
    }
  } else
    luaL_error(L, "Error in " LUA_QS ": field is not of type logical.", "xbase.writeboolean");
  return 1;
}


/* Writes the number, string or boolean (4th argument) to the file denoted by filehandle to record
   number record and field number field.

   The function automatically determines whether the respective field is of xBASE type Numeric ('N'),
   Float ('F'), binary 4-byte Long ('I'), binary 2-byte unsigned Short ('u'), binary 2-byte signed
   Short ('s'), Binary Double ('@'), Character ('C') or Logical ('L).

  The return is true if writing succeeded, and false otherwise, the latter only indicating whether
  an error may have occurred. */
static int xbase_write (lua_State *L) {  /* 2.9.8 */
  DBFHandle *hnd;
  DBFFieldType dBASEtype;
  int record, field;
  hnd = aux_gethandle(L, 1, "xbase.write");
  aux_checkpositions(L, *hnd, &record, &field, 0, "xbase.write");
  dBASEtype = DBFGetFieldInfo(*hnd, field, NULL, NULL, NULL);
  if (agn_isnumber(L, 4)) {
    lua_Number val = agn_tonumber(L, 4);
    if (dBASEtype == FTDouble  || dBASEtype == FTFloat || dBASEtype == FTBinDouble ||
        dBASEtype == FTInteger || dBASEtype == FTByte  || dBASEtype == FTDecimal   ||
        dBASEtype == FTUShort  || dBASEtype == FTShort) {
      lua_pushboolean(L, DBFWriteNumberAttribute(*hnd, record, field, val, 0));
    } else {
      luaL_error(L, "Error in " LUA_QS ": field is not of type float, decimal, byte, long, short, binary double.", "xbase.write");
    }
  } else if (lua_iscomplex(L, 4)) {
    lua_Number re, im;
    agn_getcmplxparts(L, 4, &re, &im);
    if (dBASEtype == FTBinComplex) {
      lua_pushboolean(L,
        DBFWriteNumberAttribute(*hnd, record, field, re, 0) == TRUE &&
        DBFWriteNumberAttribute(*hnd, record, field, im, sizeof(lua_Number)) == TRUE
      );
    } else
      luaL_error(L, "Error in " LUA_QS ": field is not of type double complex.", "xbase.write");

  } else if (agn_isstring(L, 4)) {
    const char *val = agn_tostring(L, 4);
    if (dBASEtype == FTString)
      lua_pushboolean(L, DBFWriteStringAttribute(*hnd, record, field, val));
    else
      luaL_error(L, "Error in " LUA_QS ": field is not of type character.", "xbase.write");
  } else if (lua_isboolean(L, 4)) {
    int val = lua_toboolean(L, 4);
    if (DBFGetFieldInfo(*hnd, field, NULL, NULL, NULL) == FTLogical) {
      switch (val) {
        case 0:
          lua_pushboolean(L, DBFWriteLogicalAttribute(*hnd, record, field, 'F'));
          break;
        case 1:
          lua_pushboolean(L, DBFWriteLogicalAttribute(*hnd, record, field, 'T'));
          break;
        default:
          luaL_error(L, "Error in " LUA_QS ": true or false as fourth argument expected.", "xbase.write");
      }
    } else
      luaL_error(L, "Error in " LUA_QS ": field is not of type logical.", "xbase.write");
  } else {
    luaL_error(L, "Error in " LUA_QS ": cannot write " LUA_QS " value.", "xbase.write", luaL_typename(L, 4));
  }
  return 1;
}


static int xbase_lock (lua_State *L) {  /* optimised 2.2.0 RC 2 */
  size_t nargs;
  int hnd;
  DBFHandle *ud;
  DBFInfo dbf;
  off64_t start, size;
  hnd = 0;
  ud = aux_gethandle(L, 1, "xbase.lock");
  dbf = **ud;
  hnd = fileno(dbf.fp);
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (nargs == 1) {  /* lock entire file (in Windows lock 2^63 bytes only) */
    start = 0;
    size = 0;
  } else {
    /* lock from current file position */
    start = my_fpos(hnd);
    size = agnL_checknumber(L, 2);
    if (size < 0) luaL_error(L, "Error in " LUA_QS ": must lock at least one byte.", "xbase.lock");
  }
  lua_pushboolean(L, my_lock(hnd, start, size) == 0);
  return 1;
}


static int xbase_unlock (lua_State *L) {  /* optimised 2.2.0 RC 2 */
  size_t nargs;
  int hnd;
  DBFHandle *ud;
  DBFInfo dbf;
  off64_t start, size;
  hnd = 0;
  ud = aux_gethandle(L, 1, "xbase.unlock");
  dbf = **ud;
  hnd = fileno(dbf.fp);
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (nargs == 1) {  /* lock entire file (in Windows lock 2^63 bytes only) */
    start = 0;
    size = 0;
  } else {
    /* lock from current file position */
    start = my_fpos(hnd);
    size = agnL_checknumber(L, 2);
    if (size < 0) luaL_error(L, "Error in " LUA_QS ": must lock at least one byte.", "xbase.unlock");
  }
  lua_pushboolean(L, my_unlock(hnd, start, size) == 0);
  return 1;
}


static int xbase_record (lua_State *L) {
  DBFHandle *hnd;
  int nrecords, nfields, recordno, *fieldinfo, i;
  char iofailure, ismarked;
  const char *result;
  lua_Number val;
  hnd = aux_gethandle(L, 1, "xbase.record");
  recordno = agnL_checkinteger(L, 2);
  nrecords = DBFGetRecordCount(*hnd);
  nfields = DBFGetFieldCount(*hnd);
  if (recordno < 1 || recordno > nrecords)
    luaL_error(L, "Error in " LUA_QS ": requested record is out of range.", "xbase.record");
  if (nrecords == 0 || nfields == 0)
    luaL_error(L, "Error in " LUA_QS ": database is empty.", "xbase.record");
  recordno--;
  if (DBFIsRecordDeleted(*hnd, recordno)) {
    lua_pushnil(L);
    return 1;
  }
  fieldinfo = malloc(nfields * sizeof(int));
  if (fieldinfo == NULL)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "xbase.record");
  for (i=0; i < nfields; i++) {
    fieldinfo[i] = DBFGetFieldInfo(*hnd, i, NULL, NULL, NULL);
  }
  agn_createseq(L, 1);
  for (i=0; i < nfields; i++) {
    switch (fieldinfo[i]) {
      case FTString: case FTMemo: case FTBinary: case FTOle: {  /* 2.31.5 fix */
        result = DBFReadStringAttribute(*hnd, recordno, i, &iofailure, &ismarked);
        if (result == NULL) {
          xfree(fieldinfo);
          luaL_error(L, "Error in " LUA_QS ": no string value stored in database.", "xbase.record");
        } else if (iofailure) {
          xfree(fieldinfo);
          luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.record");
        } else
          lua_seqrawsetistring(L, -1, i + 1, result);
        break;
      }
      case FTDate: {
        result = DBFReadDateAttribute(*hnd, recordno, i, &iofailure, &ismarked);
        if (result == NULL) {
          xfree(fieldinfo);
          luaL_error(L, "Error in " LUA_QS ": no date stored in database.", "xbase.record");
        } else if (iofailure) {
          xfree(fieldinfo);
          luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.record");
        } else
          lua_seqrawsetistring(L, -1, i + 1, result);
        break;
      }
      case FTDouble: {
        val = DBFReadDoubleAttribute(*hnd, recordno, i, &iofailure, &ismarked);
        if (iofailure) {
          xfree(fieldinfo);
          luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.record");
        } else
          agn_seqsetinumber(L, -1, i + 1, val);
        break;
      }
      case FTBinDouble: {
        val = DBFReadBinDoubleAttribute(*hnd, recordno, i, &iofailure, &ismarked);
        if (iofailure) {
          xfree(fieldinfo);
          luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.record");
        } else
          agn_seqsetinumber(L, -1, i + 1, val);
        break;
      }
      case FTInteger: {
        val = DBFReadIntegerAttribute(*hnd, recordno, i, &iofailure, &ismarked);
        if (iofailure) {
          xfree(fieldinfo);
          luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.record");
        } else
          agn_seqsetinumber(L, -1, i + 1, val);
        break;
      }
      case FTUShort: {  /* 7.3.3 */
        val = DBFReadUShortAttribute(*hnd, recordno, i, &iofailure, &ismarked);
        if (iofailure) {
          xfree(fieldinfo);
          luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.record");
        } else
          agn_seqsetinumber(L, -1, i + 1, val);
        break;
      }
      case FTShort: {  /* 7.3.3 */
        val = DBFReadShortAttribute(*hnd, recordno, i, &iofailure, &ismarked);
        if (iofailure) {
          xfree(fieldinfo);
          luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.record");
        } else
          agn_seqsetinumber(L, -1, i + 1, val);
        break;
      }
      case FTBinComplex: {  /* 2.31.6 */
        lua_Number re, im;
        re = DBFReadBinComplexAttribute(*hnd, recordno, i, &iofailure, &ismarked, 0);
        im = DBFReadBinComplexAttribute(*hnd, recordno, i, &iofailure, &ismarked, sizeof(lua_Number));
        if (iofailure) {
          xfree(fieldinfo);
          luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.record");
        } else {
          agn_pushcomplex(L, re, im);
          lua_seqrawseti(L, -2, i + 1);
        }
        break;
      }
      case FTByte: {  /* 2.31.6 */
        val = DBFReadByteAttribute(*hnd, recordno, i, &iofailure, &ismarked);
        if (iofailure) {
          xfree(fieldinfo);
          luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.record");
        } else
          agn_seqsetinumber(L, -1, i + 1, val);
        break;
      }
      case FTDecimal: {  /* 2.31.6 */
        val = DBFReadDecimalAttribute(*hnd, recordno, i, &iofailure, &ismarked);
        if (iofailure) {
          xfree(fieldinfo);
          luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.record");
        } else
          agn_seqsetinumber(L, -1, i + 1, val);
        break;
      }
      case FTTimeStamp: {  /* 2.9.8 */
        int ms;
        val = DBFReadTimeStampAttribute(*hnd, recordno, i, &iofailure, &ismarked, &ms);
        if (iofailure) {
          xfree(fieldinfo);
          luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.record");
        } else {
          agn_createpairnumbers(L, val, ms);
          lua_seqrawseti(L, -2, i + 1);
        }
        break;
      }
      case FTFloat: {
        val = DBFReadFloatAttribute(*hnd, recordno, i, &iofailure, &ismarked);
        if (iofailure) {
          xfree(fieldinfo);
          luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.record");
        } else
          agn_seqsetinumber(L, -1, i + 1, val);
        break;
      }
      case FTLogical: {
        result = DBFReadLogicalAttribute(*hnd, recordno, i, &iofailure, &ismarked);
        if (result == NULL) {
          xfree(fieldinfo);
          luaL_error(L, "Error in " LUA_QS ": no logical value stored in database.", "xbase.record");
        } else if (iofailure) {
          xfree(fieldinfo);
          luaL_error(L, "Error in " LUA_QS ": I/O failed.", "xbase.record");
        } else {
          switch (*result) {  /* 2.1.6 patch */
            case 'Y': case 'y': case 'T': case 't':
              lua_pushtrue(L);
              lua_seqrawseti(L, -2, i + 1);
              break;
            case 'N': case 'n': case 'F': case 'f':
              lua_pushfalse(L);
              lua_seqrawseti(L, -2, i + 1);
              break;
            default:
              lua_pushfail(L);
              lua_seqrawseti(L, -2, i + 1);
          }
        }
        break;
      }
      case FTInvalid: {  /* 2.31.3 */
        lua_pushstring(L, "(invalid)");
        lua_seqrawseti(L, -2, i + 1);
      }
      default: {
        xfree(fieldinfo);
        luaL_error(L, "Error in " LUA_QS ": unknown type of data.", "xbase.record");
      }
    }
  }
  free(fieldinfo);
  return 1;
}


static int xbase_filepos (lua_State *L) {
  int64_t result;
  DBFHandle *ud;
  DBFInfo dbf;
  ud = aux_gethandle(L, 1, "xbase.filepos");
  dbf = **ud;
  result = _ftelli64(dbf.fp);
  if (result == -1)
    luaL_error(L, "Error in " LUA_QS ": IO failure.", "xbase.filepos");
  lua_pushnumber(L, result);
  return 1;
}


/************************************************************************/
/*                         DBFIsRecordDeleted()                         */
/*                                                                      */
/*      Returns TRUE if the indicated record is deleted, otherwise      */
/*      it returns FALSE.                                               */
/************************************************************************/

int SHPAPI_CALL DBFIsRecordDeleted (DBFHandle psDBF, int iShape) {  /* changed Agena 2.2.0 RC 2 */
  /* Verify selection. */
  if (iShape < 0 || iShape >= psDBF->nRecords)
    return -2;  /* push fail */
  /* Have we read the record? */
  if (!DBFLoadRecord(psDBF, iShape))
    return -1;
  /* '*' means deleted. */
  return psDBF->pszCurrentRecord[0] == '*';
}


/* Returns true if a record has been marked to be deleted, false if a record has not been
   marked as such, and fail if the record number is invalid. */

static int xbase_ismarked (lua_State *L) {
  DBFHandle *hnd;
  int record, ismarked;
  hnd = aux_gethandle(L, 1, "xbase.ismarked");
  record = agn_checkinteger(L, 2) - 1;
  ismarked = DBFIsRecordDeleted(*hnd, record);
  if (ismarked == -2)
    luaL_error(L, "Error in " LUA_QS ": requested record out of range.", "xbase.ismarked");
  else
    agn_pushboolean(L, ismarked);
  return 1;
}

/************************************************************************/
/*                        DBFMarkRecordDeleted()                        */
/*  hDBF:      The access handle for the file.                       */
/*  iShape:       The record index to update.                           */
/*  bIsDeleted:   TRUE to mark record deleted, or FALSE to undelete it. */
/************************************************************************/

int SHPAPI_CALL DBFMarkRecordDeleted (DBFHandle psDBF, int iShape, int bIsDeleted) {
  char chNewFlag;
  /* Verify selection. */
  if (iShape < 0 || iShape >= psDBF->nRecords) return FALSE;
  /* Is this an existing record, but different than the last one we accessed ? */
  if (!DBFLoadRecord(psDBF, iShape)) return FALSE;
  /* Assign value, marking record as dirty if it changes. */
  chNewFlag = (bIsDeleted) ? '*' : ' ';
  if (psDBF->pszCurrentRecord[0] != chNewFlag) {
    psDBF->bCurrentRecordModified = TRUE;
    psDBF->bUpdated = TRUE;
    psDBF->pszCurrentRecord[0] = chNewFlag;
  }
  return TRUE;
}


/* Marks the specified record as to be deleted. Returns true if a record has been marked successfully,
   and false otherwise. The actual data is not deleted. This  also means that `xbase.readvalue` returns
   the values that are still stored in the record. Use `xbase.purge` to delete every entry of a record. */

static int xbase_mark (lua_State *L) {
  DBFHandle *hnd;
  int record, delete;
  hnd = aux_gethandle(L, 1, "xbase.mark");
  record = agn_checkinteger(L, 2) - 1;
  delete = agnL_optboolean(L, 3, 1);
  agn_pushboolean(L, DBFMarkRecordDeleted(*hnd, record, delete) == TRUE);
  return 1;
}


static int xbase_records (lua_State *L) {
  DBFHandle *hnd;
  hnd = aux_gethandle(L, 1, "xbase.records");
  lua_pushinteger(L, DBFGetRecordCount(*hnd));
  return 1;
}


static int xbase_fields (lua_State *L) {
  DBFHandle *hnd;
  hnd = aux_gethandle(L, 1, "xbase.fields");
  lua_pushinteger(L, DBFGetFieldCount(*hnd));
  return 1;
}


static int xbase_isopen (lua_State *L) {
  if (lua_isnoneornil(L, 1))
    lua_pushfalse(L);
  else {
    DBFHandle *hnd;
    hnd = (DBFHandle *)luaL_checkudata(L, 1, "xbase");
    lua_pushboolean(L, *hnd != NULL);
  }
  return 1;
}


static int DBFIsEof (DBFHandle psDBF) {  /* I hate these pointers ... :-) */
  FILE *f = psDBF->fp;
  return tools_eof(f);
}

static int xbase_eof (lua_State *L) {  /* 4.3.0 */
  if (lua_isnoneornil(L, 1))
    lua_pushfalse(L);
  else {
    DBFHandle *hnd = aux_gethandle(L, 1, "xbase.eof");
    lua_pushboolean(L, DBFIsEof(*hnd));
  }
  return 1;
}


static int xbase_tostring (lua_State *L) {  /* modified 2.4.2 */
  lua_pushfstring(L, "xbase(%p)", lua_topointer(L, 1));
  return 1;
}


/* Determines the dBASE data type of the given field in the open file denoted by fh. The function returns a one-character string,
   or the string '?' if it is unknown. See `xbase.new` for the meaning of the return. See also: `xbase.attrib`. */
static int xbase_fieldtype (lua_State *L) {  /* 2.9.8 */
  DBFHandle *hnd;
  DBFFieldType dBASEtype;
  int field, actfields;
  /* grep "xBASE types" if you want to change or extend this list */
  static const char *const typenames[] = {"C", "N", "F", "I", "L", "D", "O", "c", "?", "@", "M", "B", "G", "b", "f", "u", "s", NULL};
  hnd = aux_gethandle(L, 1, "xbase.fieldtype");
  actfields = DBFGetFieldCount(*hnd);
  if (actfields == 0)
    luaL_error(L, "Error in " LUA_QS ": database is empty.", "xbase.fieldtype");
  field = agnL_checkinteger(L, 2) - 1;
  if (field < 0 || field >= actfields)
    luaL_error(L, "Error in " LUA_QS ": invalid field given, must be in [1, %d].", "xbase.fieldtype", actfields);
  dBASEtype = DBFGetFieldInfo(*hnd, field, NULL, NULL, NULL);
  if (dBASEtype < 0 || dBASEtype > 9)  /* could not determine type */
    lua_pushstring(L, "?");
  else
    lua_pushstring(L, typenames[dBASEtype]);
  return 1;
}


#define unknown(L)  luaL_error(L, "Error in " LUA_QS ": unknown setting " LUA_QS ".", "xbase.kernel", lua_tostring(L, -1));

static void processoption (lua_State *L, int *var, int i, const char *option) {  /* 0.32.0 */
  agn_pairgeti(L, i, 2);  /* get right-hand side */
  if (lua_isboolean(L, -1)) {
    *var = lua_toboolean(L, -1);
    lua_remove(L, -2);  /* remove left-hand side, leave right-hand side (boolean) on stack for later return */
  } else {
    int type = lua_type(L, -1);
    agn_poptoptwo(L);  /* clear stack, 2.8.6 fix */
    luaL_error(L, "Error in " LUA_QS ": boolean for " LUA_QS " option expected, got %s.", "environ.kernel", option,
      lua_typename(L, type));
  }
}

static int xbase_kernel (lua_State *L) {  /* 2.31.5, based on environ.kernel */
  int i, nargs;
  const char *setting;
  nargs = lua_gettop(L);
  if (nargs == 0) {
    lua_createtable(L, 0, 3);
    lua_rawsetstringboolean(L, -1, "DoubleIsBigEndian", DoubleIsBigEndian);
    lua_rawsetstringboolean(L, -1, "LongIsBigEndian",   LongIsBigEndian);
    lua_rawsetstringboolean(L, -1, "TimestampIsDouble", TimestampIsDouble);
    nargs = 1;
  } else {  /* set modes */
    for (i=1; i <= nargs; i++) {
      if (lua_ispair(L, i)) {
        agn_pairgeti(L, i, 1);  /* get left-hand side */
        if (lua_type(L, -1) != LUA_TSTRING) {
          int type = lua_type(L, -1);
          agn_poptop(L);  /* clear stack */
          luaL_error(L, "Error in " LUA_QS ": string expected for left-hand side, got %s.", "xbase.kernel",
            lua_typename(L, type));
        }
        setting = lua_tostring(L, -1);
        if (tools_streq(setting, "DoubleIsBigEndian")) {
          processoption(L, &DoubleIsBigEndian, i, "DoubleIsBigEndian");
        } else if (tools_streq(setting, "LongIsBigEndian")) {
          processoption(L, &LongIsBigEndian, i, "LongIsBigEndian");
        } else if (tools_streq(setting, "TimestampIsDouble")) {
          processoption(L, &TimestampIsDouble, i, "TimestampIsDouble");
        } else
          unknown(L);
      } else if (agn_isstring(L, i)) {  /* return individual setting only */
        setting = lua_tostring(L, i);
        if (tools_streq(setting, "DoubleIsBigEndian"))
          lua_pushboolean(L, DoubleIsBigEndian);
        else if (tools_streq(setting, "LongIsBigEndian"))
          lua_pushboolean(L, LongIsBigEndian);
        else if (tools_streq(setting, "TimestampIsDouble"))
          lua_pushboolean(L, TimestampIsDouble);
        else
          unknown(L);
      } else
        luaL_error(L, "Error in " LUA_QS ": pair or string expected, got %s.", "xbase.kernel",
          luaL_typename(L, i));
    }
  }
  return nargs;
}


static int xbase_gc (lua_State *L) {  /* rewritten 2.31.9 */
  DBFHandle *hnd;
  DBFInfo dbf;
  hnd = aux_gethandle_noerror(L, 1, "(xbase gc)");
  if (*hnd == NULL) return 0;
  dbf = **hnd;
  if (dbf.fp != NULL) DBFClose(*hnd);
  return 0;
}


static const struct luaL_Reg xbase_lib [] = {
  {"attrib", xbase_attrib},
  {"close", xbase_close},
  {"eof", xbase_eof},
  {"fields", xbase_fields},
  {"fieldtype", xbase_fieldtype},
  {"filepos", xbase_filepos},
  {"header", xbase_header},
  {"ismarked", xbase_ismarked},
  {"isopen", xbase_isopen},
  {"isvoid", xbase_isvoid},
  {"lock",   xbase_lock},
  {"mark", xbase_mark},
  {"purge", xbase_purge},
  {"readdbf", xbase_readdbf},
  {"readvalue", xbase_readvalue},
  {"record", xbase_record},
  {"records", xbase_records},
  {"sync",   xbase_sync},
  {"unlock", xbase_unlock},
  {"write", xbase_write},
  {"writeboolean", xbase_writeboolean},
  {"writebyte", xbase_writebyte},
  {"writecomplex", xbase_writecomplex},
  {"writedate", xbase_writedate},
  {"writedecimal", xbase_writedecimal},
  {"writedouble", xbase_writedouble},
  {"writefloat", xbase_writefloat},
  {"writelong", xbase_writelong},
  {"writenumber", xbase_writenumber},
  {"writeshort",   xbase_writeshort},
  {"writestring", xbase_writestring},
  {"writetime", xbase_writetime},
  {"writeushort",  xbase_writeushort},
  {"__tostring", xbase_tostring},
  {"__gc", xbase_gc},
  {NULL, NULL}
};


static const luaL_Reg xbase[] = {
  {"attrib",       xbase_attrib},        /* 05.06.2010 */
  {"close",        xbase_close},         /* 05.06.2010 */
  {"eof",          xbase_eof},           /* 03.10.2024 */
  {"fields",       xbase_fields},        /* 26.05.2014 */
  {"fieldtype",    xbase_fieldtype},     /* 25.07.2016 */
  {"filepos",      xbase_filepos},       /* 13.06.2010 */
  {"header",       xbase_header},        /* 16.06.2014 */
  {"ismarked",     xbase_ismarked},      /* 10.03.2014 / 26.05.2014 */
  {"isopen",       xbase_isopen},        /* 26.05.2014 */
  {"isvoid",       xbase_isvoid},        /* 13.06.2010 */
  {"kernel",       xbase_kernel},        /* 09.09.2022 */
  {"lock",         xbase_lock},          /* 12.06.2010 */
  {"mark",         xbase_mark},          /* 10.03.2014 / 26.05.2014 */
  {"new",          xbase_new},           /* 05.06.2010 */
  {"open",         xbase_open},          /* 05.06.2010 */
  {"purge",        xbase_purge},         /* 05.06.2010 */
  {"readdbf",      xbase_readdbf},       /* 05.06.2010 */
  {"readvalue",    xbase_readvalue},     /* 05.06.2010 */
  {"record",       xbase_record},        /* 13.06.2010 */
  {"records",      xbase_records},       /* 26.05.2014 */
  {"sync",         xbase_sync},          /* 05.06.2010 */
  {"unlock",       xbase_unlock},        /* 12.06.2010 */
  {"write",        xbase_write},         /* 26.07.2016 */
  {"writeboolean", xbase_writeboolean},  /* 05.06.2010 */
  {"writebyte",    xbase_writebyte},     /* 12.09.2022 */
  {"writecomplex", xbase_writecomplex},  /* 10.09.2022 */
  {"writedate",    xbase_writedate},     /* 10.03.2014 */
  {"writedecimal", xbase_writedecimal},  /* 12.09.2022 */
  {"writedouble",  xbase_writedouble},   /* 31.05.2014 */
  {"writefloat",   xbase_writefloat},    /* 03.04.2014 */
  {"writelong",    xbase_writelong},     /* 15.07.2016 */
  {"writenumber",  xbase_writenumber},   /* 05.06.2010 */
  {"writeshort",   xbase_writeshort},    /* 28.03.2026 */
  {"writestring",  xbase_writestring},   /* 05.06.2010 */
  {"writetime",    xbase_writetime},     /* 22.07.2016 */
  {"writeushort",  xbase_writeushort},   /* 28.03.2026 */
  {NULL, NULL}
};


/*
** Open xbase library
*/

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, "xbase");      /* create metatable for file handles */
  lua_pushvalue(L, -1);               /* push metatable */
  lua_setfield(L, -2, "__index");     /* metatable.__index = metatable */
  luaL_register(L, NULL, xbase_lib);  /* methods */
}

LUALIB_API int luaopen_xbase (lua_State *L) {
  createmeta(L);
  luaL_register(L, AGENA_XBASELIBNAME, xbase);
  lua_newtable(L);
  lua_setfield(L, -2, "openfiles");  /* table for information on all open files */
  return 1;
}

