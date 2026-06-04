; Verium Miner - Windows installer (NSIS 3, Unicode)
;
; Build:  makensis -DMINER_VERSION=1.4.0 cpuminer.nsi
; Expects, in the same directory as this script:
;   cpuminer.exe          (static MinGW build, no extra DLLs)
;   cpuminer-conf.json    (example config)
;   README.md, COPYING    (docs / license)
;
; Produces: veriumminer-setup.exe

Unicode true
SetCompressor /SOLID lzma

!ifndef MINER_VERSION
  !define MINER_VERSION "0.0.0"
!endif

!define PRODUCT_NAME      "Verium Miner"
!define PRODUCT_PUBLISHER "Vericonomy"
!define PRODUCT_WEB       "https://github.com/JoshiOS-VRY/veriumMiner"
!define PRODUCT_KEY       "VeriumMiner"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}"

!include "MUI2.nsh"
!include "x64.nsh"

Name "${PRODUCT_NAME} ${MINER_VERSION}"
OutFile "veriumminer-setup.exe"
InstallDir "$PROGRAMFILES64\${PRODUCT_NAME}"
InstallDirRegKey HKLM "${UNINSTKEY}" "InstallLocation"
RequestExecutionLevel admin
BrandingText "${PRODUCT_NAME} ${MINER_VERSION}"

VIProductVersion "${MINER_VERSION}.0"
VIAddVersionKey "ProductName"     "${PRODUCT_NAME}"
VIAddVersionKey "CompanyName"     "${PRODUCT_PUBLISHER}"
VIAddVersionKey "LegalCopyright"  "Copyright (C) Vericonomy contributors"
VIAddVersionKey "FileDescription" "${PRODUCT_NAME} Setup"
VIAddVersionKey "FileVersion"     "${MINER_VERSION}"
VIAddVersionKey "ProductVersion"  "${MINER_VERSION}"

!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\cpuminer.exe"
!define MUI_FINISHPAGE_RUN_PARAMETERS "--setup"
!define MUI_FINISHPAGE_RUN_TEXT "Run the first-time setup wizard"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "Verium Miner (required)" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"
  SetOverwrite on

  File "cpuminer.exe"
  File /nonfatal "cpuminer-conf.json"
  File /nonfatal /oname=README.txt "README.md"
  File /nonfatal /oname=LICENSE.txt "COPYING"
  File /nonfatal /oname=LICENSE.txt "LICENSE"

  ; Headless service helper (Scheduled Task).
  SetOutPath "$INSTDIR\contrib"
  File /nonfatal "contrib\windows\install-service.ps1"
  SetOutPath "$INSTDIR"

  ; Shortcuts
  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\cpuminer.exe"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Setup wizard.lnk" "$INSTDIR\cpuminer.exe" "--setup"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Edit config.lnk" "notepad.exe" "$APPDATA\cpuminer\cpuminer-conf.json"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk" "$INSTDIR\uninstall.exe"

  ; Uninstaller + Add/Remove Programs entry
  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr   HKLM "${UNINSTKEY}" "DisplayName"     "${PRODUCT_NAME}"
  WriteRegStr   HKLM "${UNINSTKEY}" "DisplayVersion"  "${MINER_VERSION}"
  WriteRegStr   HKLM "${UNINSTKEY}" "Publisher"       "${PRODUCT_PUBLISHER}"
  WriteRegStr   HKLM "${UNINSTKEY}" "URLInfoAbout"    "${PRODUCT_WEB}"
  WriteRegStr   HKLM "${UNINSTKEY}" "DisplayIcon"     "$INSTDIR\cpuminer.exe"
  WriteRegStr   HKLM "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr   HKLM "${UNINSTKEY}" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\cpuminer.exe"
  Delete "$INSTDIR\cpuminer-conf.json"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\contrib\install-service.ps1"
  RMDir  "$INSTDIR\contrib"
  Delete "$INSTDIR\uninstall.exe"
  RMDir  "$INSTDIR"

  Delete "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Setup wizard.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Edit config.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk"
  RMDir  "$SMPROGRAMS\${PRODUCT_NAME}"

  DeleteRegKey HKLM "${UNINSTKEY}"
SectionEnd
