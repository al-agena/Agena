;NSIS Setup Script
;--------------------------------

Unicode True

!define APPNAME "Agena"
!define COMPANYNAME "Alexander Walz"

!ifndef VERSION
  !define VER_MAJOR '7'
  !define VER_MINOR '9'
  !define VER_REVISION '5'
  !define NICKNAME "Deimos"
  !define VERSION '${VER_MAJOR}.${VER_MINOR}.${VER_REVISION}'
!endif

;--------------------------------
; Version Information
;--------------------------------

VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=2057 "ProductName" "${APPNAME} (${NICKNAME})"
VIAddVersionKey /LANG=2057 "CompanyName" "${COMPANYNAME}"
VIAddVersionKey /LANG=2057 "LegalCopyright" "Copyright © Alexander Walz"
VIAddVersionKey /LANG=2057 "FileDescription" "${APPNAME} Interpreter for Windows"
VIAddVersionKey /LANG=2057 "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=2057 "FileVersion" "${VERSION}.0"
VIAddVersionKey /LANG=2057 "LegalTrademarks" "(n/a)"
VIAddVersionKey /LANG=2057 "OriginalFilename" "agena-${VERSION}-win32-setup.exe"
VIAddVersionKey /LANG=2057 "SpecialBuild" "${NICKNAME} Edition"
VIAddVersionKey /LANG=2057 "Comments" "Release Nickname: ${NICKNAME}"

; grep VER_SUBREVISION to activate or deactivate Library Updates

!ifndef WEBSITE
  !define WEBSITE 'http://agena.sourceforge.net'
!endif

;--------------------------------

;Configuration

BrandingText "${APPNAME} ${VERSION} ${NICKNAME}"

!ifdef OUTFILE
  OutFile "${OUTFILE}"
!else
  OutFile ..\..\agena-${VERSION}-win32-setup.exe
!endif

;Request application privileges for Windows Vista
RequestExecutionLevel user

SetCompressor /SOLID lzma

InstType "Full"
InstType "Standard"
;InstType "Minimal"

InstallDir $PROGRAMFILES\Agena
InstallDirRegKey HKLM Software\AGENA ""

RequestExecutionLevel admin

;--------------------------------

;Multi-user defines

!define MULTIUSER_EXECUTIONLEVEL Admin
;!define MULTIUSER_INSTALLMODE_DEFAULT_CURRENTUSER
!define MULTIUSER_MUI

;Header Files

!include MultiUser.nsh
!include "MUI2.nsh"
!include "Sections.nsh"
!include "LogicLib.nsh"
!include "Memento.nsh"
!include "WordFunc.nsh"
!include "StrFunc.nsh"
!include "WinMessages.nsh"

; !include "EnvVarUpdate.nsh"

;--------------------------------
;Functions

!ifdef VER_MAJOR & VER_MINOR & VER_REVISION

  !insertmacro VersionCompare

!endif

; The following NSIS 3.x functions have been taken from the document:
; `Axiom Windows Installer Script` by Jose Alfredo Perez, April 14, 2025

;====================================================
; un.IsNT - Returns 1 if the current system is NT, 0
; otherwise.
; Output: head of the stack
;====================================================

Function IsNT
Push $0
ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion
StrCmp $0 "" 0 unIsNT_yes
; we are not NT.
Pop $0
Push 0
Return

unIsNT_yes:
; NT!!!
Pop $0
Push 1
FunctionEnd

Function un.IsNT
Push $0
ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion
StrCmp $0 "" 0 unIsNT_yes
; we are not NT.
Pop $0
Push 0
Return

unIsNT_yes:
; NT!!!
Pop $0
Push 1
FunctionEnd


!macro StrStr un
Function ${un}StrStr
Exch $R1 ; st=haystack,old$R1, $R1=needle
Exch
; st=old$R1,haystack
Exch $R2 ; st=old$R1,old$R2, $R2=haystack
Push $R3
Push $R4
Push $R5
StrLen $R3 $R1
StrCpy $R4 0
; $R1=needle
; $R2=haystack
; $R3=len(needle)
; $R4=cnt
; $R5=tmp
loop:
StrCpy $R5 $R2 $R3 $R4
StrCmp $R5 $R1 done
StrCmp $R5 "" done
IntOp $R4 $R4 + 1
Goto loop
done:
StrCpy $R1 $R2 "" $R4
Pop $R5
Pop $R4
Pop $R3
Pop $R2
Exch $R1
FunctionEnd
!macroend
!insertmacro StrStr ""
!insertmacro StrStr "un."


Function AddToPath
Exch $0
Push $1
Push $2
Push $3
# don’t add if the path doesn’t exist
IfFileExists $0 "" AddToPath_done
ReadEnvStr $1 PATH
Push "$1;"
Push "$0;"
Call StrStr
Pop $2
StrCmp $2 "" "" AddToPath_done
Push "$1;"
Push "$0\;"
Call StrStr
Pop $2
StrCmp $2 "" "" AddToPath_done
GetFullPathName /SHORT $3 $0
Push "$1;"
Push "$3;"
Call StrStr
Pop $2
StrCmp $2 "" "" AddToPath_done
Push "$1;"
Push "$3\;"
Call StrStr
Pop $2
StrCmp $2 "" "" AddToPath_done
Call IsNT
Pop $1
StrCmp $1 1 AddToPath_NT
; Not on NT
StrCpy $1 $WINDIR 2
FileOpen $1 "$1\autoexec.bat" a
FileSeek $1 -1 END
FileReadByte $1 $2
IntCmp $2 26 0 +2 +2 # DOS EOF
FileSeek $1 -1 END # write over EOF
FileWrite $1 "$\r$\nSET PATH=$3;%PATH%$\r$\n"
FileClose $1
SetRebootFlag true
Goto AddToPath_done
AddToPath_NT:
ReadRegStr $1 HKCU "Environment" "PATH"
StrCpy $2 $1 1 -1 # copy last char
StrCmp $2 ";" 0 +2 # if last char == ;
StrCpy $1 $1 -1 # remove last char
StrCmp $1 "" AddToPath_NTdoIt
StrCpy $0 "$0;$1"
AddToPath_NTdoIt:
WriteRegExpandStr HKCU "Environment" "PATH" $0
SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000
AddToPath_done:
Pop $3
Pop $2
Pop $1
Pop $0
FunctionEnd


Function un.RemoveFromPath
Exch $0
Push $1
Push $2
Push $3
Push $4
Push $5
Push $6
IntFmt $6 "%c" 26 # DOS EOF
Call un.IsNT
Pop $1
StrCmp $1 1 unRemoveFromPath_NT
; Not on NT
StrCpy $1 $WINDIR 2
FileOpen $1 "$1\autoexec.bat" r
GetTempFileName $4
FileOpen $2 $4 w
GetFullPathName /SHORT $0 $0
StrCpy $0 "SET PATH=%PATH%;$0"
Goto unRemoveFromPath_dosLoop
unRemoveFromPath_dosLoop:
FileRead $1 $3
StrCpy $5 $3 1 -1 # read last char
StrCmp $5 $6 0 +2 # if DOS EOF
StrCpy $3 $3 -1 # remove DOS EOF so we can compare
StrCmp $3 "$0$\r$\n" unRemoveFromPath_dosLoopRemoveLine
StrCmp $3 "$0$\n" unRemoveFromPath_dosLoopRemoveLine
StrCmp $3 "$0" unRemoveFromPath_dosLoopRemoveLine
StrCmp $3 "" unRemoveFromPath_dosLoopEnd
FileWrite $2 $3
Goto unRemoveFromPath_dosLoop
unRemoveFromPath_dosLoopRemoveLine:
SetRebootFlag true
Goto unRemoveFromPath_dosLoop
unRemoveFromPath_dosLoopEnd:
FileClose $2
FileClose $1
StrCpy $1 $WINDIR 2
Delete "$1\autoexec.bat"
CopyFiles /SILENT $4 "$1\autoexec.bat"
Delete $4
Goto unRemoveFromPath_done
unRemoveFromPath_NT:
ReadRegStr $1 HKCU "Environment" "PATH"
StrCpy $5 $1 1 -1 # copy last char
StrCmp $5 ";" +2 # if last char != ;
StrCpy $1 "$1;" # append ;
Push $1
Push "$0;"
Call un.StrStr ; Find ‘$0;‘in $1
Pop $2 ; pos of our dir
StrCmp $2 "" unRemoveFromPath_done
; else, it is in path
# $0 - path to add
# $1 - path var
StrLen $3 "$0;"
StrLen $4 $2
StrCpy $5 $1 -$4 # $5 is now the part before the path to remove
StrCpy $6 $2 "" $3 # $6 is now the part after the path to remove
StrCpy $3 $5$6
StrCpy $5 $3 1 -1 # copy last char
StrCmp $5 ";" 0 +2 # if last char == ;
StrCpy $3 $3 -1 # remove last char
WriteRegExpandStr HKCU "Environment" "PATH" $3
SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000
unRemoveFromPath_done:
Pop $6
Pop $5
Pop $4
Pop $3
Pop $2
Pop $1
Pop $0
FunctionEnd


;--------------------------------
;Definitions

!define SHCNE_ASSOCCHANGED 0x8000000
!define SHCNF_IDLIST 0

;--------------------------------
;Configuration

;Names
Name "Agena"
Caption "Agena ${VERSION} Setup"

BGGradient 000000 800000 FFFFFF
InstallColors FF8080 000030
XPStyle off

!define MUI_HEADERIMAGE_BITMAP "..\nsis\banner.bmp"

!define MUI_COMPONENTSPAGE_CHECKBITMAP "${NSISDIR}\Contrib\Graphics\Checks\classic.bmp"

!define MUI_ICON "..\..\share\icons\agena256.ico"

;Memento Settings
!define MEMENTO_REGISTRY_ROOT HKLM
!define MEMENTO_REGISTRY_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\AGENA"

;Interface Settings
!define MUI_ABORTWARNING

!define MUI_HEADERIMAGE
!define MUI_WELCOMEFINISHPAGE_BITMAP "..\nsis\logo.bmp"

!define MUI_COMPONENTSPAGE_SMALLDESC

;Pages
!define MUI_WELCOMEPAGE_TITLE "Welcome to the$\r$\nAgena ${VERSION} Setup Wizard"
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of the$\r$\nAgena ${VERSION} ${NICKNAME} Interpreter.$\r$\n$\rAgena is an easy-to-learn procedural programming language designed to be used in scientific, educational, linguistic, network, and many other applications.$\r$\n$\rIts syntax looks like very simplified Algol 68 with elements taken from Lua, Maple and SQL.$\r$\n$\rThe implementation is based on the original Lua 5.1 and 5.4 sources written by Roberto Ierusalimschy, Luiz Henrique de Figueiredo, and Waldemar Celes.$\r$\n$\r$\n$_CLICK"

;LicenseForceSelection checkbox "I accept."

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\licence"
!ifdef VER_MAJOR & VER_MINOR & VER_REVISION
Page custom PageReinstall PageLeaveReinstall
!endif

!insertmacro MUI_PAGE_WELCOME
;  !define MUI_PAGE_CUSTOMFUNCTION_PRE multiuser_pre_func
!insertmacro MULTIUSER_PAGE_INSTALLMODE

!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY

;-----------------------------------------------------------------------------------------
;Start Menu Folder Page
;-----------------------------------------------------------------------------------------
;Variables
Var StartMenuFolder
!define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKCU"
!define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\Agena"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"

!insertmacro MUI_PAGE_STARTMENU Application $StartMenuFolder

;-----------------------------------------------------------------------------------------
; Installation Page
;-----------------------------------------------------------------------------------------
ShowInstDetails show
!insertmacro MUI_PAGE_INSTFILES

;-----------------------------------------------------------------------------------------
; Finish page
;-----------------------------------------------------------------------------------------
!define MUI_FINISHPAGE_LINK "Visit the Agena site for the latest news, add-ons and updates."
!define MUI_FINISHPAGE_LINK_LOCATION "${WEBSITE}"

!define MUI_FINISHPAGE_RUN_NOTCHECKED
!define MUI_FINISHPAGE_RUN "$INSTDIR\bin\agena.exe"
!define MUI_FINISHPAGE_NOREBOOTSUPPORT

!define MUI_FINISHPAGE_SHOWREADME
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!define MUI_FINISHPAGE_SHOWREADME_TEXT "View Crash Course"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION ShowReleaseNotes

!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

;--------------------------------
;Languages

!insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections


${MementoSection} "Agena Core Files (required)" SecCore

  ;SetDetailsPrint textonly
  SetDetailsPrint both
  DetailPrint "Installing Agena Core Files ..."
  ;SetDetailsPrint listonly

  SectionIn 1 2 RO
  SetOutPath $INSTDIR
  RMDir /r $SMPROGRAMS\Agena

  SetOutPath $INSTDIR\bin
  File ..\..\src\agena.exe
  File ..\..\share\icons\agena256.ico
  File ..\..\share\icons\aedit256.ico
  File ..\..\src\agena.dll
  File ..\..\licence
  File ..\..\change.log

; General dependencies:
  File ..\..\ports\gdwin32\bin\libgmp-10.dll
  File ..\..\ports\gdwin32\bin\libmpfr-6.dll
  File ..\..\ports\gdwin32\bin\libstdc++-6.dll
  File ..\..\ports\gdwin32\bin\zlib1.dll
; reported by Slobodan (gzip and gdi need this lib):
  File ..\..\ports\gdwin32\bin\libgcc_s_dw2-1.dll
  File ..\..\ports\gdwin32\bin\libmingwex-4.dll
; needed by gzip & gdi:
  File ..\..\ports\gdwin32\bin\libmingwex-0.dll
; The following libiconv(-)2 copies are needed, especially by the `gdi` and `gzip` packages;
; depending on the Windows version either one or the other is needed:
  File ..\..\ports\gdwin32\bin\libiconv2.dll
; libiconv-2.dll is needed by the `iconv` package
  File ..\..\ports\gdwin32\bin\libiconv-2.dll
; regex
  File ..\..\ports\gdwin32\bin\libpcre2-8-0.dll
  File ..\..\ports\gdwin32\bin\libpcre2-posix-3.dll
; xml
  File ..\..\ports\gdwin32\bin\libexpat-1.dll

  SetOutPath $INSTDIR\lib
  File ..\..\lib\library.agn
  File ..\..\lib\ansi.agn
  File ..\..\lib\agenaini.spl

${MementoSectionEnd}


${MementoSection} "AgenaEdit" SecAgenaEdit
  SectionIn 1 2

  SetOutPath $INSTDIR\bin
  File ..\..\..\fltk-1.4.5\editor\agenaedit.exe

${MementoSectionEnd}


${MementoSection} "Documentation" SecDoc
  SectionIn 1 2

  SetOutPath $INSTDIR\doc
  File ..\..\doc\agena-primer.pdf
  File ..\..\doc\agena-reference.pdf
  File ..\..\doc\agena-crashcourse.pdf
  File ..\..\doc\agena-quickref.xls
  File ..\..\doc\ascii.xls
  File ..\..\doc\regex.txt

${MementoSectionEnd}

!define AGENAPATH "$INSTDIR\lib"
!define AGENASCRIPT "$INSTDIR\share\scripting"

${MementoSection} "Set Environment Variable AGENAPATH" SecEnv
;Section "Set Environment Variable AGENAPATH"
  SectionIn 1 2

  !ifdef ALL_USERS
    !define ReadEnvStr_RegKey \
       'HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"'
  !else
    !define ReadEnvStr_RegKey 'HKCU "Environment"'
  !endif

  Push AGENAPATH
  Push "${AGENAPATH}"
  ;Call WriteEnvStr
  ;StrCpy $R0 "$INSTDIR"
  StrCpy $R0 "${AGENAPATH}"
  ;messagebox mb_ok '$R0'
  System::Call 'Kernel32::SetEnvironmentVariableA(t, t) i("AGENAPATH", R0).r2'

  ReadEnvStr $R0 "PATH"
  ;messagebox mb_ok '$R0'
  ;ensure that is written valid for NT only
  ReadRegStr $0 ${ReadEnvStr_RegKey} 'AGENAPATH'
  StrCpy $R0 "$R0;$0"
  System::Call 'Kernel32::SetEnvironmentVariableA(t, t) i("PATH", R0).r2'
  ReadEnvStr $R0 "PATH"
  ;messagebox mb_ok '$R0'
  ;writeuninstaller '$EXEDIR\uninst.exe'

  ;permanently set AGENAPATH
  WriteRegExpandStr HKCU "Environment" "AGENAPATH" "${AGENAPATH}"

${MementoSectionEnd}


${MementoUnselectedSection} "Append path to Agena binary to PATH" SecEnvPath
; Section /o "Append Agena folder to PATH"
  SectionIn 1 2

  ;append main Agena folder to PATH
  ;${EnvVarUpdate} $0 "PATH" "A" "HKLM" "$INSTDIR\bin"

  Push "$INSTDIR\bin"
  Call AddToPath

${MementoSectionEnd}


${MementoSection} "Desktop Shortcut" SecShortcuts

  SetDetailsPrint both
  DetailPrint "Installing Desktop Shortcut ..."
  SetDetailsPrint listonly

  SectionIn 1 2
  SetOutPath $INSTDIR

  CreateShortCut "$DESKTOP\agena.lnk" "$INSTDIR\bin\agena.exe" "" "$INSTDIR\bin\agena256.ico"

${MementoSectionEnd}

SectionGroup "plus Packages" SecPluginsPlugins


${MementoSection} "astro" SecPluginsAstro

  SetDetailsPrint both
  DetailPrint "Installing plus package astro ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\astro.dll
  File ..\..\lib\astro.agn

  SetOutPath $INSTDIR\data
  File ..\..\data\cities.kdx
  File ..\..\data\cities.dbf
  File ..\..\data\cities.txt
  File ..\..\data\country.csv
  File ..\..\data\timezone.csv

${MementoSectionEnd}


${MementoSection} "cordic" SecPluginsCordic

  SetDetailsPrint both
  DetailPrint "Installing plus package cordic ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\cordic.dll

${MementoSectionEnd}


${MementoSection} "divs" SecPluginsDivs

  SetDetailsPrint both
  DetailPrint "Installing plus package divs ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\divs.agn

${MementoSectionEnd}


${MementoSection} "fastmath" SecPluginsFastmath

  SetDetailsPrint both
  DetailPrint "Installing plus package fastmath ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\fastmath.dll

${MementoSectionEnd}


${MementoSection} "ival" SecPluginsIval

  SetDetailsPrint both
  DetailPrint "Installing plus package ival ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\ival.agn
  File ..\..\lib\ival.dll

${MementoSectionEnd}


${MementoSection} "mapm" SecPluginsMapm

  SetDetailsPrint both
  DetailPrint "Installing plus package mapm ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\mapm.dll
  File ..\..\lib\mapm.agn

${MementoSectionEnd}


${MementoSection} "gmp" SecPluginsGmp

  SetDetailsPrint both
  DetailPrint "Installing plus package gmp ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\gmp.dll
  File ..\..\lib\gmp.agn

${MementoSectionEnd}


${MementoSection} "mpfr" SecPluginsMpfr

  SetDetailsPrint both
  DetailPrint "Installing plus package mpfr ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\mpfr.dll
  File ..\..\lib\mpfr.agn

${MementoSectionEnd}


${MementoSection} "unimath" SecPluginsUnimath

  SetDetailsPrint both
  DetailPrint "Installing plus package unimath ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\unimath.agn

${MementoSectionEnd}


${MementoSection} "kiss" SecPluginsKiss

  SetDetailsPrint both
  DetailPrint "Installing plus package kiss ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\kiss.agn

${MementoSectionEnd}


${MementoSection} "zx" SecPluginsZX

  SetDetailsPrint both
  DetailPrint "Installing plus package zx ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\zx.dll

${MementoSectionEnd}


${MementoSection} "maple" SecPluginsMaple

  SetDetailsPrint both
  DetailPrint "Installing plus package maple ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\maple.agn

${MementoSectionEnd}


${MementoSection} "fractals" SecPluginsFractals

  SetDetailsPrint both
  DetailPrint "Installing plus package fractals ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\fractals.dll
  File ..\..\lib\fractals.agn

${MementoSectionEnd}


${MementoSection} "gdi" SecPluginsGdi

  SetDetailsPrint both
  DetailPrint "Installing plus package gdi ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\gdi.agn
  File ..\..\lib\gdi.dll

  SetOutPath $INSTDIR\bin
  File ..\..\ports\gdwin32\bin\libfreetype-6.dll
  File ..\..\ports\gdwin32\bin\libfontconfig-1.dll
  File ..\..\ports\gdwin32\bin\libjpeg-10.dll
  File ..\..\ports\gdwin32\bin\libpng16-16.dll
  File ..\..\ports\gdwin32\bin\xpm4.dll
  File ..\..\ports\gdwin32\bin\libgd.dll
; In W2K and XP we need TIFF dependencies
  File ..\..\ports\gdwin32\bin\libtiff-6.dll
  File ..\..\ports\gdwin32\bin\libtiffxx-6.dll

${MementoSectionEnd}


${MementoSection} "curses" SecPluginsCurses

  SetDetailsPrint both
  DetailPrint "Installing plus package curses ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
; File ..\..\lib\curses.agn
  File ..\..\lib\curses.dll

  SetOutPath $INSTDIR\bin
; The following is needed by the curses package (tinfo is not needed):
  File ..\..\ports\gdwin32\bin\libpanel6.dll
  File ..\..\ports\gdwin32\bin\libncurses6.dll
  File ..\..\ports\gdwin32\bin\libncurses++6.dll
  File ..\..\ports\gdwin32\bin\libmenu6.dll
  File ..\..\ports\gdwin32\bin\libform6.dll

${MementoSectionEnd}


${MementoSection} "com" SecPluginsCom

  SetDetailsPrint both
  DetailPrint "Installing plus package com ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\com.dll

${MementoSectionEnd}


${MementoSection} "net" SecPluginsNet

  SetDetailsPrint both
  DetailPrint "Installing plus package Internet & LAN package net (IPv4) ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\net.dll
  File ..\..\lib\net.agn

${MementoSectionEnd}


${MementoSection} "usb" SecPluginsUsb

  SetDetailsPrint both
  DetailPrint "Installing plus package usb ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\usb.dll
  ;File ..\..\lib\usb.agn

  SetOutPath $INSTDIR\bin
  File ..\..\ports\libusb-1.0.18\libusb\.libs\libusb-1.0.dll

${MementoSectionEnd}


${MementoSection} "ads" SecPluginsADS

  SetDetailsPrint both
  DetailPrint "Installing plus package Agena Database System ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\ads.dll
  File ..\..\lib\ads.agn

${MementoSectionEnd}


${MementoSection} "gzip" SecPluginsGzip

  SetDetailsPrint both
  DetailPrint "Installing plus package gzip ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\bin
  File ..\..\ports\gdwin32\bin\zlib1.dll

  SetOutPath $INSTDIR\lib
  File ..\..\lib\gzip.dll

${MementoSectionEnd}


${MementoSection} "minizip" SecPluginsMinizip

  SetDetailsPrint both
  DetailPrint "Installing plus package minizip ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\minizip.dll

${MementoSectionEnd}


${MementoSection} "tar" SecPluginsTar

  SetDetailsPrint both
  DetailPrint "Installing plus package tar ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\tar.agn

${MementoSectionEnd}


${MementoSection} "sqlite" SecPluginsSqlite

  SetDetailsPrint both
  DetailPrint "Installing plus package sqlite ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\sqlite.dll

  SetOutPath $INSTDIR\doc
  File ..\..\doc\SQLite.htm
  File ..\..\doc\SQLite.agn

${MementoSectionEnd}


${MementoSection} "skycrane" SecPluginsSkycrane

  SetDetailsPrint both
  DetailPrint "Installing plus package skycrane ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\skycrane.dll
  File ..\..\lib\skycrane.agn

${MementoSectionEnd}


${MementoSection} "strhash" SecPluginsStrhash

  SetDetailsPrint both
  DetailPrint "Installing plus package strhash ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\strhash.dll

${MementoSectionEnd}


${MementoSection} "strmap" SecPluginsStrmap

  SetDetailsPrint both
  DetailPrint "Installing plus package strmap ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\strmap.dll

${MementoSectionEnd}


${MementoSection} "iconv" SecPluginsIconv

  SetDetailsPrint both
  DetailPrint "Installing plus package iconv ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\iconv.dll
  File ..\..\lib\iconv.agn

; The following is needed by the iconv package:
  SetOutPath $INSTDIR\bin

${MementoSectionEnd}


${MementoSection} "abci" SecPluginsAbci

  SetDetailsPrint both
  DetailPrint "Installing byte code inspector ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\abci.dll
  File ..\..\lib\abci.agn

${MementoSectionEnd}


${MementoSection} "testlib" SecPluginsTestlib

  SetDetailsPrint both
  DetailPrint "Installing reference test functions in testlib package ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\testlib.dll

${MementoSectionEnd}


${MementoSection} "double" SecPluginsDouble

  SetDetailsPrint both
  DetailPrint "Installing userdata demo functions in double package ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\lib
  File ..\..\lib\double.dll
  File ..\..\lib\double.agn

${MementoSectionEnd}


;${MementoSectionDone}

SectionGroupEnd

SectionGroup "Misc" SecMisc

${MementoSection} "Editor scheme files" SecMiscSchemas

  SetDetailsPrint both
  DetailPrint "Installing scheme files for some editors ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\share\schemes
  File ..\..\share\schemes\nedit.rc
  File ..\..\share\schemes\nedit.rc.solaris
  File ..\..\share\schemes\agena.lang
  File ..\..\share\schemes\agena.sch
  File ..\..\share\schemes\agena.xml
  File ..\..\share\schemes\agena.dat
  File ..\..\share\schemes\readme.txt

${MementoSectionEnd}

${MementoSection} "Further icons" SecMiscIcons

  SetDetailsPrint both
  DetailPrint "Installing further icons ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\share\icons
  File ..\..\share\icons\agena8b.ico
  File ..\..\share\icons\agenasmall.ico
  File ..\..\share\icons\agena128x128.ico
  File ..\..\share\icons\agena64x64.ico
  File ..\..\share\icons\agena256.ico
  File ..\..\share\icons\aedit256.ico

${MementoSectionEnd}

${MementoSection} "Scripting examples" SecMiscScripting

  SetDetailsPrint both
  DetailPrint "Installing scripting examples ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\share\scripting
  File ..\..\share\scripting\getopt.agn
  File ..\..\share\scripting\ln.agn
  File ..\..\share\scripting\cpuinfo.agn
  File ..\..\share\scripting\cpuinfo.bat
  File ..\..\share\scripting\drive.agn
  File ..\..\share\scripting\drive.bat
  File ..\..\share\scripting\memory.agn
  File ..\..\share\scripting\memory.bat
  File ..\..\share\scripting\whereis.agn
  File ..\..\share\scripting\whereis.bat

  SetOutPath $INSTDIR\lib
  File ..\..\lib\telex.agn

${MementoSectionEnd}

${MementoSection} "FRACTINT colour maps" SecMiscFractint

  SetDetailsPrint both
  DetailPrint "Installing FRACTINT colour maps ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\share\fractint

  File ..\..\share\fractint\alamo.map
  File ..\..\share\fractint\bagheram.map
  File ..\..\share\fractint\bordeaux.map
  File ..\..\share\fractint\brgtsaph.map
  File ..\..\share\fractint\dancooper.map
  File ..\..\share\fractint\donald.map
  File ..\..\share\fractint\drowning.map
  File ..\..\share\fractint\hafez.map
  File ..\..\share\fractint\hafez2.map
  File ..\..\share\fractint\m-maybe.map
  File ..\..\share\fractint\maga.map
  File ..\..\share\fractint\mondrian.map
  File ..\..\share\fractint\oasis.map
  File ..\..\share\fractint\office.map
  File ..\..\share\fractint\office2.map
  File ..\..\share\fractint\orgteal.map
  File ..\..\share\fractint\orozil.map
  File ..\..\share\fractint\orozil2.map
  File ..\..\share\fractint\rauschen.map
  File ..\..\share\fractint\sapphire.map
  File ..\..\share\fractint\teahouse.map
  File ..\..\share\fractint\usa.map
  File ..\..\share\fractint\velvet.map
  File ..\..\share\fractint\y2ktechn.map
  File ..\..\share\fractint\chloro.map
  File ..\..\share\fractint\chloro2.map
  File ..\..\share\fractint\cinemat.map
  File ..\..\share\fractint\fa18raaf.map
  File ..\..\share\fractint\fa18usaf.map
  File ..\..\share\fractint\h2o.map
  File ..\..\share\fractint\h2o2.map
  File ..\..\share\fractint\h2o3.map
  File ..\..\share\fractint\h2o4.map
  File ..\..\share\fractint\h2o5.map
  File ..\..\share\fractint\h2o6.map
  File ..\..\share\fractint\iaf.map
  File ..\..\share\fractint\lime.map
  File ..\..\share\fractint\painter.map
  File ..\..\share\fractint\paintr2.map
  File ..\..\share\fractint\pluto.map
  File ..\..\share\fractint\prism.map
  File ..\..\share\fractint\prism2.map
  File ..\..\share\fractint\prism3.map
  File ..\..\share\fractint\prism4.map
  File ..\..\share\fractint\prism5.map
  File ..\..\share\fractint\spectra2.map
  File ..\..\share\fractint\spectra3.map
  File ..\..\share\fractint\spectra4.map
  File ..\..\share\fractint\spectral.map
  File ..\..\share\fractint\tiranga.map
  File ..\..\share\fractint\bordsprg.map
  File ..\..\share\fractint\rand01.map
  File ..\..\share\fractint\rand02.map
  File ..\..\share\fractint\rand03.map
  File ..\..\share\fractint\rand04.map
  File ..\..\share\fractint\alexmaps.txt

${MementoSectionEnd}

${MementoSection} "Data files" SecMiscData

  SetDetailsPrint both
  DetailPrint "Installing data files to play with ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\data
  File ..\..\data\langreg.csv
  File ..\..\data\viking2.csv
  File ..\..\data\viking2.txt
  File ..\..\data\airlines.csv
  File ..\..\data\airlines.txt
  File ..\..\data\sunspots.csv
  File ..\..\data\sunspots.txt
  File ..\..\data\gothic.csv

${MementoSectionEnd}

${MementoSection} "Self-running Agena interpreter" SecMiscSrglue

  SetDetailsPrint both
  DetailPrint "Installing srglue and sragena utilities ..."
  SetDetailsPrint listonly

  SectionIn 1

  SetOutPath $INSTDIR\bin

  File ..\..\ports\sragena-105\bin\sragena.exe
  File ..\..\ports\sragena-105\bin\srglue.exe
  File ..\..\ports\sragena-105\README.srglue

${MementoSectionEnd}

SectionGroupEnd

${MementoSectionDone}


;!include WriteEnvStr.nsh

Section -post

  SetDetailsPrint textonly
  DetailPrint "Creating Registry Keys ..."
  SetDetailsPrint listonly

  SetOutPath $INSTDIR

  ;WriteRegExpandStr HKCU "Environment" "AGENAPATH" "$INSTDIR"
  WriteRegStr HKLM "Software\Agena" "" $INSTDIR
!ifdef VER_MAJOR & VER_MINOR & VER_REVISION
  WriteRegDword HKLM "Software\Agena" "VersionMajor" "${VER_MAJOR}"
  WriteRegDword HKLM "Software\Agena" "VersionMinor" "${VER_MINOR}"
  WriteRegDword HKLM "Software\Agena" "VersionRevision" "${VER_REVISION}"
!endif

  WriteRegExpandStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegExpandStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "Publisher" "${COMPANYNAME}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "DisplayName" "${APPNAME}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "DisplayIcon" "$INSTDIR\bin\agena256.ico,0"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "DisplayVersion" "${VERSION}"
  ; See -post section
  ;WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "EstimatedSize"  "${EST_SIZE}"
!ifdef VER_MAJOR & VER_MINOR & VER_REVISION
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "VersionMajor" "${VER_MAJOR}"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "VersionMinor" "${VER_MINOR}"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "VersionRevision" "${VER_REVISION}"
!endif
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "URLInfoAbout" "${WEBSITE}/"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "HelpLink" "http://agena.sourceforge.net"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "NoModify" "1"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "NoRepair" "1"

  WriteUninstaller $INSTDIR\uninstall.exe

  ${MementoSectionSave}

  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application

    ;Create shortcuts
    CreateDirectory "$SMPROGRAMS\$StartMenuFolder"
    CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Agena.lnk" "$INSTDIR\bin\agena.exe" "" "$INSTDIR\share\icons\agena256.ico"
    CreateShortCut "$SMPROGRAMS\$StartMenuFolder\AgenaEdit.lnk" "$INSTDIR\bin\agenaedit.exe" "" "$INSTDIR\share\icons\aedit256.ico"
    CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Primer.lnk" "$INSTDIR\doc\agena-primer.pdf"
    CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Reference.lnk" "$INSTDIR\doc\agena-reference.pdf"
    CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Crash Course.lnk" "$INSTDIR\doc\agena-crashcourse.pdf"
    CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Quick Reference.lnk" "$INSTDIR\doc\agena-quickref.xls"
    CreateShortCut "$SMPROGRAMS\$StartMenuFolder\ASCII.lnk" "$INSTDIR\doc\ascii.xls"
    CreateShortCut "$SMPROGRAMS\$StartMenuFolder\regex.lnk" "$INSTDIR\doc\regex.txt"
    CreateShortCut "$SMPROGRAMS\$StartMenuFolder\SQLite.lnk" "$INSTDIR\doc\SQLite.htm"
    CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Change Log.lnk" "$INSTDIR\bin\change.log"
    CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Licence.lnk" "$INSTDIR\bin\licence"
    WriteINIStr "$SMPROGRAMS\$StartMenuFolder\Agena @ Sourceforge.url" "InternetShortcut" "URL" ${WEBSITE}
    ;CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Agena @ Sourceforge.lnk" "${WEBSITE}" "URL"
    CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Uninstall.lnk" "$INSTDIR\uninstall.exe"

  !insertmacro MUI_STARTMENU_WRITE_END

  SetDetailsPrint both

SectionEnd

;--------------------------------
;Descriptions

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecCore} "The core files required to use Agena (interpreter and main Agena library)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecAgenaEdit} "The Agena editor and runtime environment"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDoc} "The manual, language summary, and quick reference"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecEnv} "Permanently sets the environment variable AGENAPATH to your system (recommended)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecEnvPath} "Appends Agena path to system PATH"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecShortcuts} "Adds Agena icon to your desktop for easy access"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsPlugins} "Additonal libraries"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsAstro} "Astronomical time and date package"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsCordic} "CORDIC (COordinate Rotation DIgital Computer) numeric package"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsKiss} "Fast Fourier Transform package (KissFFT)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsFastmath} "Fastmath, numerical approximations"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsDivs} "Fraction package"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsMapm} "Michael C. Ring's Arbitrary Precision Math library (MAPM)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsGmp} "GNU Multiple Precision Arithmetic Library (GMP)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsMpfr} "GNU Multiple Precision Floating-Point Reliable Library (MPFR)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsUnimath} "Common functions for floating-point objects: numbers, long doubles, MAPM, MPFR"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsIval} "Ival arithmetic"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsZX} "Sinclair ZX Spectrum math function clones"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsKiss} "Aliases for Maple functions (in construction)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsGdi} "GDI package, for drawing graphics"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsCurses} "GNU (n)curses package, for making good-looking terminal apps"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsFractals} "Fractals package"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsNet} "Network package for sending data across the Internet and LANs"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsCom} "RS-232 serial communication via COM ports"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsUsb} "libusb 1.0 binding to access USB devices"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsADS} "Agena Database System package, a small text data base"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsGzip} "UNIX gzip compression library"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsMinizip} "ZIP compression library"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsTar} "UNIX tape archive library"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsSqlite} "SQLite3 database library"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsSkycrane} "Auxiliary Functions"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsIconv} "GNU iconv internationalization conversion port"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsAbci} "Byte code inspector (see abci.agn for a demo)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsTestlib} "Reference test functions"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsDouble} "Userdata demonstration"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsStrhash} "String hash set"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPluginsStrmap} "String-to-number hash map"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMisc} "Miscellaneous"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMiscSchemas} "Editor scheme files (installs to $INSTDIR\share\schemes)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMiscIcons} "Further icons (installs to $INSTDIR\share)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMiscScripting} "Scripting examples (installs to $INSTDIR\share\scripting)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMiscFractint} "FRACTINT colour maps (installs to $INSTDIR\share\fractint)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMiscData} "Data files to play around (sunspots, Viking meteo data, flight schedule)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMiscSrglue} "Utility to convert a script file into a self-running executable"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
;Installer Functions

Function .onInit
  !insertmacro MULTIUSER_INIT
  ${MementoSectionRestore}
  # the plugins dir is automatically deleted when the installer exits
  InitPluginsDir
  # Installation for all users
  SetShellVarContext all
  # Check whether installer is already running
  System::Call 'kernel32::CreateMutexA(i 0, i 0, t "myMutex") i .r1 ?e'
  Pop $R0
  StrCmp $R0 0 +3
    MessageBox MB_OK|MB_ICONEXCLAMATION "The installer is already running."
    Abort
  # Check for previous version
;  ReadRegStr $R0 HKLM \
;  "Software\Microsoft\Windows\CurrentVersion\Uninstall\AGENA" \
;  "UninstallString"
;  StrCmp $R0 "" checkdone
;  MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
;  "Agena is already installed. $\n$\nClick OK to remove the previous version \
;  or Cancel to cancel this installer." \
;  IDOK uninst
;  Abort
;uninst:
;  ClearErrors
;  ExecWait '$R0 _?=$INSTDIR'  ; Do not copy the uninstaller to a temp file
;  IfErrors no_remove_uninstaller checkdone
;    ; some command here
;   no_remove_uninstaller:

;checkdone:
  # Show splash image
  File /oname=$PLUGINSDIR\splash.bmp "..\nsis\splash.bmp"
  advsplash::show 2000 600 300 -1 $PLUGINSDIR\splash
  Pop $0          ; $0 has '1' if the user closed the splash screen early,
                  ; '0' if everything closed normally, and '-1' if some error occurred.
  Delete $PLUGINSDIR\splash.bmp
FunctionEnd


Function un.onInit
 !insertmacro MULTIUSER_UNINIT
  SetShellVarContext all
FunctionEnd


!ifdef VER_MAJOR & VER_MINOR & VER_REVISION

Var ReinstallPageCheck

Function PageReinstall
  ReadRegStr $R0 HKLM "Software\Agena" ""
  ${If} $R0 == ""
    Abort
  ${EndIf}
  ReadRegDWORD $R0 HKLM "Software\Agena" "VersionMajor"
  ReadRegDWORD $R1 HKLM "Software\Agena" "VersionMinor"
  ReadRegDWORD $R2 HKLM "Software\Agena" "VersionRevision"
  StrCpy $R4 $R0.$R1.$R2
  StrCpy $R0 $R0.$R1.$R2
  ${VersionCompare} ${VER_MAJOR}.${VER_MINOR}.${VER_REVISION} $R0 $R0
  ${If} $R0 == 0
    StrCpy $R1 "Agena ${VERSION} is already installed. Select the operation you want to perform and click Next to continue."
    StrCpy $R2 "Add/Reinstall components"
    StrCpy $R3 "Uninstall Agena"
    !insertmacro MUI_HEADER_TEXT "Already Installed" "Choose the maintenance option to perform."
    StrCpy $R0 "2"
  ${ElseIf} $R0 == 1
    StrCpy $R1 "This will replace Agena $R4 that is currently installed on your system. It's recommended that you uninstall the current version before installing. Select the operation you want to perform and click Next to continue."
    StrCpy $R2 "Uninstall before installing"
    StrCpy $R3 "Do not uninstall"
    !insertmacro MUI_HEADER_TEXT "Already Installed" "Choose how you want to install Agena."
    StrCpy $R0 "1"
  ${ElseIf} $R0 == 2
    StrCpy $R1 "The newer Agena version $R4 is currently installed. It is not recommended that you install an older version. If you really want to install this older version, it's better to uninstall the current version first. Select the operation you want to perform and click Next to continue."
    StrCpy $R2 "Uninstall before installing"
    StrCpy $R3 "Do not uninstall"
    !insertmacro MUI_HEADER_TEXT "Already Installed" "Choose how you want to install Agena."
    StrCpy $R0 "1"
  ${Else}
    Abort
  ${EndIf}

  nsDialogs::Create /NOUNLOAD 1018
  Pop $R4
  ${NSD_CreateLabel} 0 0 100% 24u $R1
  Pop $R1
  ${NSD_CreateRadioButton} 30u 50u -30u 8u $R2
  Pop $R2
  ${NSD_OnClick} $R2 PageReinstallUpdateSelection
  ${NSD_CreateRadioButton} 30u 70u -30u 8u $R3
  Pop $R3
  ${NSD_OnClick} $R3 PageReinstallUpdateSelection
  ${If} $ReinstallPageCheck != 2
    SendMessage $R2 ${BM_SETCHECK} ${BST_CHECKED} 0
  ${Else}
    SendMessage $R3 ${BM_SETCHECK} ${BST_CHECKED} 0
  ${EndIf}
  nsDialogs::Show
FunctionEnd

Function PageReinstallUpdateSelection
  Pop $R1
  ${NSD_GetState} $R2 $R1
  ${If} $R1 == ${BST_CHECKED}
    StrCpy $ReinstallPageCheck 1
  ${Else}
    StrCpy $ReinstallPageCheck 2
  ${EndIf}
FunctionEnd

Function PageLeaveReinstall
  ${NSD_GetState} $R2 $R1
  StrCmp $R0 "1" 0 +2
  StrCmp $R1 "1" reinst_uninstall reinst_done
  StrCmp $R0 "2" 0 +3
  StrCmp $R1 "1" reinst_done reinst_uninstall

  reinst_uninstall:
  ReadRegStr $R1 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AGENA" "UninstallString"

  ;Run uninstaller
  ; HideWindow
  ClearErrors
  ExecWait '$R1 _?=$INSTDIR'

  IfErrors no_remove_uninstaller
  IfFileExists "$INSTDIR\makensis.exe" no_remove_uninstaller

  Delete $R1
  RMDir $INSTDIR

  no_remove_uninstaller:

  StrCmp $R0 "2" 0 +2
  Quit

  BringToFront
  reinst_done:
FunctionEnd

!endif # VER_MAJOR & VER_MINOR & VER_REVISION

Function ShowReleaseNotes
  ExecShell "open" "$INSTDIR\doc\agena-crashcourse.pdf"
FunctionEnd

;--------------------------------
;Uninstaller Section

UninstallText "This will uninstall Agena from your computer.$\n$\nThanks for using it."
UninstallCaption "Uninstall Agena"

Section "Uninstall"

  SetDetailsPrint textonly
  DetailPrint "Uninstalling Agena ..."
  SetDetailsPrint listonly

  IfFileExists $INSTDIR\bin\agena.exe agena_installed
    MessageBox MB_YESNO "It does not appear that Agena is installed in the directory '$INSTDIR'.$\r$\nContinue anyway (not recommended)?" IDYES agena_installed
    Abort "Uninstall aborted by user"
  agena_installed:

  SetDetailsView show
  DetailPrint "Deleting Registry Keys ..."
  SetDetailsPrint listonly

  ReadRegStr $R0 HKCR ".nsi" ""
  StrCmp $R0 "NSIS.Script" 0 +2
  DeleteRegKey HKCR ".nsi"

  ReadRegStr $R0 HKCR ".nsh" ""
  StrCmp $R0 "NSIS.Header" 0 +2
  DeleteRegKey HKCR ".nsh"

  DeleteRegKey HKCR "NSIS.Script"
  DeleteRegKey HKCR "NSIS.Header"

  System::Call 'Shell32::SHChangeNotify(i ${SHCNE_ASSOCCHANGED}, i ${SHCNF_IDLIST}, i 0, i 0)'

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AGENA"
  DeleteRegKey HKLM "Software\AGENA"

  DetailPrint "Deleting Files ..."

  Delete $SMPROGRAMS\agena.lnk
  Delete $DESKTOP\agena.lnk
  RMDir /r $INSTDIR\bin
  RMDir /r $INSTDIR\doc

  Delete $INSTDIR\lib\*.agn
  Delete $INSTDIR\lib\*.dll
  Delete $INSTDIR\lib\agenaini.spl

  ;remove lib only if it is empty
  RMDir $INSTDIR\lib
  RMDIr /r $INSTDIR\src
  Delete $INSTDIR\data\langreg.csv
  Delete $INSTDIR\data\viking2.csv
  Delete $INSTDIR\data\viking2.txt
  Delete $INSTDIR\data\airlines.csv
  Delete $INSTDIR\data\airlines.txt
  Delete $INSTDIR\data\sunspots.csv
  Delete $INSTDIR\data\sunspots.txt
  Delete $INSTDIR\data\gothic.csv
  RMDir $INSTDIR\data

  Delete $INSTDIR\share\fractint\*
  Delete $INSTDIR\share\icons\*
  Delete $INSTDIR\share\schemes\*
  Delete $INSTDIR\share\scripting\*
  RMDIr $INSTDIR\share\fractint
  RMDIr $INSTDIR\share\icons
  RMDIr $INSTDIR\share\schemes
  RMDIr $INSTDIR\share\scripting
  ;RMDIr /r $INSTDIR\share
  RMDIr $INSTDIR\share
  ; Remove uninstaller
  Delete $INSTDIR\uninstall.exe
  ;RMDir /r $INSTDIR
  RMDir $INSTDIR

  !include "WinMessages.nsh" ; Required to define the WM_WININICHANGE constant
  DeleteRegValue HKCU "Environment" "AGENAPATH"
  ; Notify Windows of the change
  SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000

  ;Remove Agena folder from PATH
  ;${un.EnvVarUpdate} $0 "PATH" "R" "HKLM" "$INSTDIR\bin"

  Push "$INSTDIR\bin"
  Call un.RemoveFromPath

  ;1. Calculate the size of the core section (and others if you wish)
  ; This stores the size in kilobytes into variable $0
  SectionGetSize ${SecCore} $0

  ; If you want to add other sections to the total:
  SectionGetSize ${SecAgenaEdit} $1
  IntOp $0 $0 + $1
  SectionGetSize ${SecDoc} $1
  IntOp $0 $0 + $1

  ; 2. Update the registry call to use $0 instead of ${EST_SIZE}
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "EstimatedSize" $0
  ; Write the dynamically calculated size to the registry
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "EstimatedSize" $0

  !insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuFolder
  ;Delete "$SMPROGRAMS\$StartMenuFolder\Agena.lnk"
  ;Delete "$SMPROGRAMS\$StartMenuFolder\Manual.lnk"
  ;Delete "$SMPROGRAMS\$StartMenuFolder\Quick Reference.lnk"
  ;Delete "$SMPROGRAMS\$StartMenuFolder\Agena @ Sourceforge.lnk"
  ;Delete "$SMPROGRAMS\$StartMenuFolder\Uninstall.lnk"
  Delete "$SMPROGRAMS\$StartMenuFolder\*.*"
  RMDir "$SMPROGRAMS\$StartMenuFolder"

SectionEnd