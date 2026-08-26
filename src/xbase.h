#ifndef xbase_h
#define xbase_h

/******************************************************************************
 * $Id: shapefil.h,v 1.26 2002/09/29 00:00:08 warmerda Exp $
 *
 * Project:  Shapelib
 * Purpose:  Primary include file for Shapelib.
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
 * $Log: shapefil.h,v $
 * Revision 1.26  2002/09/29 00:00:08  warmerda
 * added FTLogical and logical attribute read/write calls
 *
 * Revision 1.25  2002/05/07 13:46:30  warmerda
 * added DBFWriteAttributeDirectly().
 *
 * Revision 1.24  2002/04/10 16:59:54  warmerda
 * added SHPRewindObject
 *
 * Revision 1.23  2002/01/15 14:36:07  warmerda
 * updated email address
 *
 * Revision 1.22  2002/01/15 14:32:00  warmerda
 * try to improve SHPAPI_CALL docs
 *
 * Revision 1.21  2001/11/01 16:29:55  warmerda
 * move pabyRec into SHPInfo for thread safety
 *
 * Revision 1.20  2001/07/20 13:06:02  warmerda
 * fixed SHPAPI attribute for SHPTreeFindLikelyShapes
 *
 * Revision 1.19  2001/05/31 19:20:13  warmerda
 * added DBFGetFieldIndex()
 *
 * Revision 1.18  2001/05/31 18:15:40  warmerda
 * Added support for NULL fields in DBF files
 *
 * Revision 1.17  2001/05/23 13:36:52  warmerda
 * added use of SHPAPI_CALL
 *
 * Revision 1.16  2000/09/25 14:15:59  warmerda
 * added DBFGetNativeFieldType()
 *
 * Revision 1.15  2000/02/16 16:03:51  warmerda
 * added null shape support
 *
 * Revision 1.14  1999/11/05 14:12:05  warmerda
 * updated license terms
 *
 * Revision 1.13  1999/06/02 18:24:21  warmerda
 * added trimming code
 *
 * Revision 1.12  1999/06/02 17:56:12  warmerda
 * added quad'' subnode support for trees
 *
 * Revision 1.11  1999/05/18 19:11:11  warmerda
 * Added example searching capability
 *
 * Revision 1.10  1999/05/18 17:49:38  warmerda
 * added initial quadtree support
 *
 * Revision 1.9  1999/05/11 03:19:28  warmerda
 * added new Tuple api, and improved extension handling - add from candrsn
 *
 * Revision 1.8  1999/03/23 17:22:27  warmerda
 * Added extern "C" protection for C++ users of shapefil.h.
 *
 * Revision 1.7  1998/12/31 15:31:07  warmerda
 * Added the TRIM_DBF_WHITESPACE and DISABLE_MULTIPATCH_MEASURE options.
 *
 * Revision 1.6  1998/12/03 15:48:15  warmerda
 * Added SHPCalculateExtents().
 *
 * Revision 1.5  1998/11/09 20:57:16  warmerda
 * Altered SHPGetInfo() call.
 *
 * Revision 1.4  1998/11/09 20:19:33  warmerda
 * Added 3D support, and use of SHPObject.
 *
 * Revision 1.3  1995/08/23 02:24:05  warmerda
 * Added support for reading bounds.
 *
 * Revision 1.2  1995/08/04  03:17:39  warmerda
 * Added header.
 *
 */

/* Byte 0 of header -> version number,
   see: http://stackoverflow.com/questions/3391525/which-header-format-can-be-assumed-by-reading-an-initial-dbf-byte

-----------
x xxx x 001 = 0x?1 not used
0 000 0 010 = 0x02 FoxBASE
0 000 0 011 = 0x03 FoxBASE+/dBASE III PLUS, no memo
x xxx x 100 = 0x?4 dBASE 7
x xxx x 111 = 0x?7 agenaBASE Level 1
0 000 0 101 = 0x05 dBASE 5, no memo
0 011 0 000 = 0x30 Visual FoxPro
0 011 0 001 = 0x31 Visual FoxPro, autoincrement enabled
0 011 0 010 = 0x32 Visual FoxPro, Varchar, Varbinary, or Blob-enabled
0 100 0 011 = 0x43 dBASE IV SQL table files, no memo
0 110 0 011 = 0x63 dBASE IV SQL system files, no memo
0 111 1 011 = 0x7B dBASE IV, with memo
1 000 0 011 = 0x83 FoxBASE+/dBASE III PLUS, with memo
1 000 1 011 = 0x8B dBASE IV, with memo
1 000 1 110 = 0x8E dBASE IV with SQL table
1 100 1 011 = 0xCB dBASE IV SQL table files, with memo
1 110 0 101 = 0xE5 Clipper SIX driver, with SMT memo
1 111 0 101 = 0xF5 FoxPro 2.x (or earlier) with memo
1 111 1 011 = 0xFB FoxBASE (with memo?)
| ||| | |||
| ||| | |||   Bit flags (not used in all formats)
| ||| | |||   -----------------------------------
| ||| | +++-- bits 2, 1, 0, version (x03 = level 5, x04 = level 7)
| ||| +------ bit 3, presence of memo file
| +++-------- bits 6, 5, 4, presence of dBASE IV SQL table
+------------ bit 7, presence of .DBT file */


#include <stdio.h>
#include <stdint.h>  /* for int32_t */

#ifdef USE_DBMALLOC
#include <dbmalloc.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************/
/*                        Configuration options.                        */
/************************************************************************/

/* -------------------------------------------------------------------- */
/*      Should the DBFReadStringAttribute() strip leading and           */
/*      trailing white space?                                           */
/* -------------------------------------------------------------------- */
#define TRIM_DBF_WHITESPACE

/* -------------------------------------------------------------------- */
/*      Should we write measure values to the Multipatch object?        */
/*      Reportedly ArcView crashes if we do write it, so for now it     */
/*      is disabled.                                                    */
/* -------------------------------------------------------------------- */
#define DISABLE_MULTIPATCH_MEASURE

/* -------------------------------------------------------------------- */
/*      SHPAPI_CALL                                                     */
/*                                                                      */
/*      The following two macros are present to allow forcing           */
/*      various calling conventions on the Shapelib API.                */
/*                                                                      */
/*      To force __stdcall conventions (needed to call Shapelib         */
/*      from Visual Basic and/or Dephi I believe) the makefile could    */
/*      be modified to define:                                          */
/*                                                                      */
/*        /DSHPAPI_CALL=__stdcall                                       */
/*                                                                      */
/*      If it is desired to force export of the Shapelib API without    */
/*      using the shapelib.def file, use the following definition.      */
/*                                                                      */
/*        /DSHAPELIB_DLLEXPORT                                          */
/*                                                                      */
/*      To get both at once it will be necessary to hack this           */
/*      include file to define:                                         */
/*                                                                      */
/*        #define SHPAPI_CALL __declspec(dllexport) __stdcall           */
/*        #define SHPAPI_CALL1 __declspec(dllexport) * __stdcall        */
/*                                                                      */
/*      The complexity of the situation is partly caused by the         */
/*      peculiar requirement of Visual C++ that __stdcall appear        */
/*      after any "*"'s in the return value of a function while the     */
/*      __declspec(dllexport) must appear before them.                  */
/* -------------------------------------------------------------------- */

#ifdef SHAPELIB_DLLEXPORT
#  define SHPAPI_CALL      __declspec(dllexport)
#  define SHPAPI_CALL1(x)  __declspec(dllexport) x
#endif

#ifndef SHPAPI_CALL
#  define SHPAPI_CALL
#endif

#ifndef SHPAPI_CALL1
#  define SHPAPI_CALL1(x)      x SHPAPI_CALL
#endif


/************************************************************************/
/*                             DBF Support.                             */
/************************************************************************/

typedef struct {  /* regrouped 7.3.3 */
  /* --- 8-Byte Members (Pointers and Doubles) --- */
  double dfDoubleField;     /* 2.1.6 */
  FILE   *fp;
  int    *panFieldOffset;
  int    *panFieldSize;
  int    *panFieldDecimals;
  char   *pachFieldType;
  char   *pszHeader;
  char   *pszCurrentRecord;
  char   *filename;
  char   *pszCodePage;      /* 2.1.6 */
  /* --- 4-Byte Members (Integers) --- */
  int    nRecords;
  int    nRecordLength;
  int    nHeaderLength;
  int    nFields;
  int    nCurrentRecord;
  int    bCurrentRecordModified;
  int    bNoHeader;
  int    bUpdated;
  int    iLanguageDriver;  /* 2.1.6 */
  int    lastmodified;
  int    dBaseVersion;     /* 2.2.0 RC 3 */
  /* Optimization: 4 bytes of tail padding may be added by 
     the compiler to keep the total struct size a multiple of 8 */
} DBFInfo;

typedef DBFInfo *DBFHandle;

typedef enum {  /* grep "xBASE types" if you want to change or extend this list */
  FTString,
  FTDouble,
  FTFloat,
  FTInteger,    /* 2.9.8 */
  FTLogical,
  FTDate,
  FTBinDouble,
  FTBinComplex, /* 2.31.6 */
  FTInvalid,
  FTTimeStamp,  /* 2.9.8 */
  FTMemo,       /* 2.31.3 */
  FTBinary,     /* 2.31.3 */
  FTOle,        /* 2.31.3 */
  FTByte,       /* 2.31.6 */
  FTDecimal,    /* 2.31.6 */
  FTUShort,     /* 7.3.3 */
  FTShort       /* 7.3.3 */
} DBFFieldType;

#define XBASE_FLDHDR_SZ       32

DBFHandle SHPAPI_CALL DBFOpen (const char * pszDBFFile, const char * pszAccess);
DBFHandle SHPAPI_CALL DBFCreate (const char * pszDBFFile);

int  SHPAPI_CALL DBFGetFieldCount (DBFHandle psDBF);
int  SHPAPI_CALL DBFGetRecordCount (DBFHandle psDBF);
int  SHPAPI_CALL DBFAddField (DBFHandle hDBF, const char * pszFieldName, DBFFieldType eType, int nWidth, int nDecimals );
DBFFieldType SHPAPI_CALL DBFGetFieldInfo (DBFHandle psDBF, int iField, char *pszFieldName, int *pnWidth, int *pnDecimals);

double  SHPAPI_CALL DBFReadDoubleAttribute (DBFHandle hDBF, int iShape, int iField, char *iofailure, char *ismarked);
double  SHPAPI_CALL DBFReadBinDoubleAttribute (DBFHandle hDBF, int iShape, int iField, char *iofailure, char *ismarked);  /* 2.2.0 RC 3 */
double  SHPAPI_CALL DBFReadBinComplexAttribute (DBFHandle hDBF, int iShape, int iField, char *iofailure, char *ismarked, int offset);  /* 2.31.6 */
double  SHPAPI_CALL DBFReadDecimalAttribute (DBFHandle psDBF, int iRecord, int iField, char *iofailure, char *ismarked);
double  SHPAPI_CALL DBFReadFloatAttribute (DBFHandle hDBF, int iShape, int iField, char *iofailure, char *ismarked);  /* 2.1.6 */
double  SHPAPI_CALL DBFReadIntegerAttribute (DBFHandle hDBF, int iShape, int iField, char *iofailure, char *ismarked);  /* 2.9.8 */
double  SHPAPI_CALL DBFReadUShortAttribute (DBFHandle hDBF, int iShape, int iField, char *iofailure, char *ismarked);  /* 7.3.3 */
double  SHPAPI_CALL DBFReadShortAttribute (DBFHandle hDBF, int iShape, int iField, char *iofailure, char *ismarked);  /* 7.3.3 */
double  SHPAPI_CALL DBFReadByteAttribute (DBFHandle psDBF, int iRecord, int iField, char *iofailure, char *ismarked);  /* 2.31.6 */
double  SHPAPI_CALL DBFReadTimeStampAttribute (DBFHandle psDBF, int iRecord, int iField, char *iofailure, char *ismarked, int *ms); /* 2.9.8 */

const char SHPAPI_CALL1(*) DBFReadStringAttribute (DBFHandle hDBF, int iShape, int iField, char *iofailure, char *ismarked);
const char SHPAPI_CALL1(*) DBFReadLogicalAttribute (DBFHandle hDBF, int iShape, int iField, char *iofailure, char *ismarked);
const char SHPAPI_CALL1(*) DBFReadDateAttribute (DBFHandle hDBF, int iShape, int iField, char *iofailure, char *ismarked);  /* 2.1.6 */

int SHPAPI_CALL DBFIsAttributeNULL ( DBFHandle hDBF, int iShape, int iField);
int SHPAPI_CALL DBFIsRecordDeleted (DBFHandle psDBF, int iShape);

int SHPAPI_CALL DBFWriteTimeStampAttribute (DBFHandle psDBF, int iRecord, int iField, uint64_t dValue);  /* 2.9.8 */
int SHPAPI_CALL DBFWriteNumberAttribute (DBFHandle hDBF, int iShape, int iField, double dFieldValue, int offset);
int SHPAPI_CALL DBFWriteDateAttribute (DBFHandle hDBF, int iShape, int iField, double dFieldValue);    /* 2.1.6 */
int SHPAPI_CALL DBFWriteStringAttribute (DBFHandle hDBF, int iShape, int iField, const char * pszFieldValue);
int SHPAPI_CALL DBFWriteLogicalAttribute (DBFHandle hDBF, int iShape, int iField, const char lFieldValue);

int  SHPAPI_CALL DBFClose (DBFHandle hDBF);
char SHPAPI_CALL DBFGetNativeFieldType (DBFHandle hDBF, int iField);

int DBFLoadRecord (DBFHandle psDBF, int iRecord);
int DBFFlushRecord (DBFHandle psDBF);

#ifdef __cplusplus
}
#endif

#endif /* ndef _SHAPEFILE_H_INCLUDED */
