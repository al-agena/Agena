;-----------------------------------------------------------------------------------------
; Agena Portable Simplified User Installer; created with the help of Gemini AI
;-----------------------------------------------------------------------------------------
Unicode True
!define APPNAME "Agena"
!define VERSION "7.9.2"
!define NICKNAME "Deimos"
!define COMPANYNAME "Alexander Walz"
Name "${APPNAME} ${VERSION}"
OutFile "..\..\agena-${VERSION}-win32-portable.exe"

;-----------------------------------------------------------------------------------------
; Version Information
;-----------------------------------------------------------------------------------------

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

!ifndef WEBSITE
  !define WEBSITE 'http://agena.sourceforge.net'
!endif

; Request user-level privileges
RequestExecutionLevel user

; Default install to LocalAppData
InstallDir "$LOCALAPPDATA\Agena ${VERSION}"

!include "MUI2.nsh"
!include "Sections.nsh"

;-----------------------------------------------------------------------------------------
; Configuration
;-----------------------------------------------------------------------------------------

BrandingText "${APPNAME} ${VERSION} ${NICKNAME}"

;Request application privileges for Windows Vista
RequestExecutionLevel user

SetCompressor /SOLID lzma

;-----------------------------------------------------------------------------------------
; Layout
;-----------------------------------------------------------------------------------------

Caption "Agena ${VERSION} Portable Setup"

BGGradient 000000 000080 FFFFFF
InstallColors AACCFF 000040

!define MUI_HEADERIMAGE_BITMAP "..\nsis\banner.bmp"
!define MUI_ICON "..\..\share\icons\agena256.ico"

!define MUI_COMPONENTSPAGE_CHECKBITMAP "${NSISDIR}\Contrib\Graphics\Checks\classic.bmp"

;Memento Settings
!define MEMENTO_REGISTRY_ROOT HKLM
!define MEMENTO_REGISTRY_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\AGENA"

;-----------------------------------------------------------------------------------------
; Dialogues
;-----------------------------------------------------------------------------------------

;!define MUI_ABORTWARNING

!define MUI_HEADERIMAGE
!define MUI_WELCOMEFINISHPAGE_BITMAP "..\nsis\logo.bmp"

!define MUI_COMPONENTSPAGE_SMALLDESC

!define MUI_ABORTWARNING
!define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKCU"
!define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\Agena"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"

; Use this to change the main bold header at the top
!define MUI_DIRECTORYPAGE_TEXT_TOP "This 2-click installer lets you set up Agena without administrative rights and leaves your system environment variables unchanged. It installs Agena and its documentation, the AgenaEdit editor, all libraries plus data and schema files.$\n$\nThe Agena ${VERSION} binary distribution is released under the GNU Library Public Licence,$\nversion 2 and later. By installing this software, you agree to the terms of the licence provided in the installation directory."
!define MUI_DIRECTORYPAGE_TEXT_DESTINATION "Agena Installation Folder"

; Call the directory page once
!insertmacro MUI_PAGE_DIRECTORY

; Proceed to file installation
!insertmacro MUI_PAGE_INSTFILES

;-----------------------------------------------------------------------------------------
; Finish page
;-----------------------------------------------------------------------------------------
!define MUI_FINISHPAGE_LINK "Visit the Agena site for the latest news, add-ons and updates."
!define MUI_FINISHPAGE_LINK_LOCATION "${WEBSITE}"

!define MUI_FINISHPAGE_RUN_NOTCHECKED
!define MUI_FINISHPAGE_RUN "$INSTDIR\run.exe"
!define MUI_FINISHPAGE_NOREBOOTSUPPORT

!define MUI_FINISHPAGE_SHOWREADME
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!define MUI_FINISHPAGE_SHOWREADME_TEXT "View Crash Course"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION ShowReleaseNotes

!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

;!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

;-----------------------------------------------------------------------------------------
; Installation Section
;-----------------------------------------------------------------------------------------
Section "MainSection" SecCore
  SectionIn RO

  ; File installation
  SetOutPath "$INSTDIR\bin"
  File /r "..\..\..\winprogs\agena\bin\*"
  SetOutPath "$INSTDIR\lib"
  File /r "..\..\..\winprogs\agena\lib\*"
  SetOutPath "$INSTDIR\data"
  File /r "..\..\..\winprogs\agena\data\*"
  SetOutPath "$INSTDIR\doc"
  File /r "..\..\..\winprogs\agena\doc\*"
  SetOutPath "$INSTDIR\share"
  File /r "..\..\..\winprogs\agena\share\*"
  SetOutPath "$INSTDIR"
  File "..\..\..\winprogs\agena\run.exe"
  File "..\..\..\winprogs\agena\change.log"
  File "..\..\..\winprogs\agena\licence"

; Create uninstaller
  WriteUninstaller "$INSTDIR\uninstall.exe"

; Force the shell to use current user context
  SetShellVarContext current

  CreateDirectory "$SMPROGRAMS\Agena ${VERSION}"

  CreateShortCut "$DESKTOP\Agena ${VERSION}.lnk" "$INSTDIR\run.exe" "" "$INSTDIR\share\icons\agena256.ico"

;-----------------------------------------------------------------------------------------
; Shortcuts
;-----------------------------------------------------------------------------------------
  CreateShortCut "$SMPROGRAMS\Agena ${VERSION}\Agena.lnk" "$INSTDIR\run.exe" "" "$INSTDIR\share\icons\agena256.ico"
  CreateShortCut "$SMPROGRAMS\Agena ${VERSION}\AgenaEdit.lnk" "$INSTDIR\bin\agenaedit.exe" "" "$INSTDIR\share\icons\aedit256.ico"
  CreateShortCut "$SMPROGRAMS\Agena ${VERSION}\Primer.lnk" "$INSTDIR\doc\agena-primer.pdf"
  CreateShortCut "$SMPROGRAMS\Agena ${VERSION}\Reference.lnk" "$INSTDIR\doc\agena-reference.pdf"
  CreateShortCut "$SMPROGRAMS\Agena ${VERSION}\Crash Course.lnk" "$INSTDIR\doc\agena-crashcourse.pdf"
  CreateShortCut "$SMPROGRAMS\Agena ${VERSION}\Quick Reference.lnk" "$INSTDIR\doc\agena-quickref.xls"
  CreateShortCut "$SMPROGRAMS\Agena ${VERSION}\Change Log.lnk" "$INSTDIR\change.log"
  CreateShortCut "$SMPROGRAMS\Agena ${VERSION}\Licence.lnk" "$INSTDIR\licence"
  WriteINIStr    "$SMPROGRAMS\Agena ${VERSION}\Agena @ Sourceforge.url" "InternetShortcut" "URL" "${WEBSITE}"
  CreateShortCut "$SMPROGRAMS\Agena ${VERSION}\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0

  ; Registry Keys (User-level)
  WriteRegStr HKCU "Software\Agena" "" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "DisplayName" "${APPNAME} ${VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "DisplayIcon" "$INSTDIR\share\icons\agena256.ico,0"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "Publisher" "${COMPANYNAME}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena" "UninstallString" "$INSTDIR\uninstall.exe"
SectionEnd

Function ShowReleaseNotes
  ExecShell "open" "$INSTDIR\doc\agena-crashcourse.pdf"
FunctionEnd

;-----------------------------------------------------------------------------------------
; Uninstaller
;-----------------------------------------------------------------------------------------
Section "Uninstall"
  ; Remove Desktop Shortcut
  Delete "$DESKTOP\Agena ${VERSION}.lnk"
  ; Remove Start Menu Shortcuts and Directory
  Delete "$SMPROGRAMS\Agena ${VERSION}\Agena.lnk"
  Delete "$SMPROGRAMS\Agena ${VERSION}\AgenaEdit.lnk"
  Delete "$SMPROGRAMS\Agena ${VERSION}\Primer.lnk"
  Delete "$SMPROGRAMS\Agena ${VERSION}\Reference.lnk"
  Delete "$SMPROGRAMS\Agena ${VERSION}\Crash Course.lnk"
  Delete "$SMPROGRAMS\Agena ${VERSION}\Quick Reference.lnk"
  Delete "$SMPROGRAMS\Agena ${VERSION}\Change Log.lnk"
  Delete "$SMPROGRAMS\Agena ${VERSION}\Licence.lnk"
  Delete "$SMPROGRAMS\Agena ${VERSION}\Agena @ Sourceforge.url"
  Delete "$SMPROGRAMS\Agena ${VERSION}\Uninstall.lnk"
  RMDir  "$SMPROGRAMS\Agena ${VERSION}"
  ; Remove Installation files and Registry keys
  Delete "$INSTDIR\uninstall.exe"
  RMDir /r "$INSTDIR"
  DeleteRegKey HKCU "Software\Agena"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Agena"
SectionEnd
