; kDrive Recovery Updater - NSIS self-extracting installer
; Extracts files to a temp directory and runs the updater, then cleans up.

!include "MUI2.nsh"

Name "kDrive Recovery Updater"
OutFile "@{output}"
InstallDir "$TEMP\kDriveRecoveryUpdater"
RequestExecutionLevel user
ShowInstDetails hide
ShowUnInstDetails hide
CRCCheck On

!define MUI_ICON "@{icon}"
!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

VIProductVersion "@{version}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "kDrive Recovery Updater"
VIAddVersionKey /LANG=${LANG_ENGLISH} "CompanyName" "Infomaniak Network SA"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "kDrive Recovery Updater"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "@{version}"

Section
    SetOutPath "$INSTDIR"
    File /r "@{staging}\*.*"

    ExecWait '"$INSTDIR\kDriveRecoveryUpdater.exe"'
SectionEnd

Function .onInstSuccess
    ; Clean up extracted files after the updater exits
    RMDir /r /REBOOTOK "$INSTDIR"
FunctionEnd
