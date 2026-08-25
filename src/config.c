/* Determination of Endianness of your system and the size of a C double.
   This file is used in the compilation process, do not delete it. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* strlen, strrchr, strcmp */
#include <ctype.h>  /* toupper */
#include <time.h>

#define uchar(c) ((unsigned char)(c))

#define ENDIAN_VERSION   "%s v0.1.3 as of January 25, 2026 - (C) by Alexander Walz\n"


void Usage (char *progname) {
   fprintf(stderr, "\nUsage:    %s [<option>] [<filename>]\n\n", "config");
   fprintf(stderr, "Option:   -h  this information\n\n");
   fprintf(stderr, "The function determines the endianness of your system. It outputs the strings\n");
   fprintf(stderr, "\"#define BIG_ENDIAN 4321\", \"#define LITTLE_ENDIAN 1234\", and\n");
   fprintf(stderr, "\"#define BYTE_ORDER LITTLE_ENDIAN\" or \"#define BYTE_ORDER BIG_ENDIAN\".\n");
   fprintf(stderr, "depending on the endianness of your system.\n\n");
   fprintf(stderr, "If a file name is given, this string is written to the given file and not to\nstdout.\n\n");
   fprintf(stderr, "This tool comes with no warranty.\n");
}


int HandleOptions (int argc, char *argv[]) {
   int i, firstnonoption = 1;
   for (i=1; i < argc; i++) {
      if (strcmp(argv[i], "--help") == 0) {
         Usage(argv[0]);
         exit(1);
      }
      if (argv[i][0] == '-') {
         switch (argv[i][1]) {
            case '?': case 'h':
               Usage(argv[0]);
               exit(1);
            default:
               fprintf(stderr,"unknown option %s\n",argv[i]);
               fflush(stderr);
               break;
         }
      } else {
         firstnonoption = i;
         break;
      }
   }
   return firstnonoption;
}


void printdatetime (FILE *fp) {
   struct tm *stm;
   time_t seconds;
   int year, month, day, hour, minute;
   char *months[] = {
      "none", "January", "February", "March", "April", "May", "June", "July", "August",
      "September", "October", "November", "December" };
   seconds = time(NULL);
   stm = localtime(&seconds);  /* get local time */
   if (stm != NULL) {  /* 0.31.3 patch */
     year = stm->tm_year+1900;
     month = stm->tm_mon+1;
     day = stm->tm_mday;
     hour = stm->tm_hour;
     minute = stm->tm_min;
   } else {
     year = 1900; month = 1; day = 1; hour = 0; minute = 0;
   }
   fprintf(fp, "#define AGENA_BUILDDATE  \"%s %02d, %4d\"\n", months[month], day, year);
   fprintf(fp, "#define AGENA_BUILDTIME  \"%02d:%02d h\"\n", hour, minute);
}


int main (int argc, char *argv[]) {
  int x, y, firstnonopt, filenamegiven;
  FILE *fp;
  x = 1;
  y = *(char*)&x;
  firstnonopt = HandleOptions(argc, argv);
  filenamegiven = (argc > firstnonopt);
  fp = filenamegiven ? fopen(argv[argc-1], "wt") : stdout;
  if (fp == NULL) {
    fprintf(stderr, "Error, cannot open file.\n");
    return EXIT_FAILURE;
  }
  fprintf(fp, "#ifndef agncfg_h\n"
              "#define agncfg_h\n\n"
              "#ifdef BYTE_ORDER\n"
              "#  undef BYTE_ORDER\n"
              "#endif\n"
              "#ifdef LITTLE_ENDIAN\n"
              "#  undef LITTLE_ENDIAN\n"
              "#endif\n"
              "#ifdef BIG_ENDIAN\n"
              "#  undef BIG_ENDIAN\n"
              "#endif\n\n"
              "#define BIG_ENDIAN	4321\n"
              "#define LITTLE_ENDIAN	1234\n");
  fprintf(fp, "#define BYTE_ORDER %s\n", y ? "LITTLE_ENDIAN" : "BIG_ENDIAN");
  /* determine correct version of tools_swaplong function in agnhlps.c */
  fprintf(fp, "#ifdef ACTUAL_SIZE_OF_C_LONG\n"
              "#  undef ACTUAL_SIZE_OF_C_LONG\n"
              "#endif\n");
/* DJGPP v2 has problems with %d and size_t data, so use `%ld' placeholder */
/* 2.25.5 HOTFIX for Raspi 400 */
#if defined(__DJGPP__) || defined(__APPLE__) || defined(__HAIKU__)
  fprintf(fp, "#define ACTUAL_SIZE_OF_C_LONG	%ld\n", sizeof(long));
#else
  fprintf(fp, "#define ACTUAL_SIZE_OF_C_LONG	%d\n", (int)sizeof(long));
#endif
  fprintf(fp, "#ifdef NECESSARY_SIZE_OF_C_LONG\n"
              "#  undef NECESSARY_SIZE_OF_C_LONG\n"
              "#endif\n"
              "#define NECESSARY_SIZE_OF_C_LONG	8\n");
  printdatetime(fp);
  fprintf(fp, "\n#endif\n");
  fclose(fp);
  return 0;
}


