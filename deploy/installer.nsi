; ============================================================================
; BigFileViewer NSIS Installer Script
; ============================================================================
; Prerequisites:
;   - NSIS 3.x installed (https://nsis.sourceforge.io/)
;   - Run deploy_win.bat first to create the Release folder
;
; Build installer:
;   makensis installer.nsi
; ============================================================================

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"

; ============================================================================
; Application Info
; ============================================================================
!define APP_NAME "BigFileViewer"
!define APP_VERSION "1.0.0"
!define APP_PUBLISHER "BigFileViewer Team"
!define APP_URL "https://bigfileviewer.com"
!define APP_EXE "BigFileViewer.exe"

; Registry keys for uninstaller
!define REG_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"

; ============================================================================
; Installer Attributes
; ============================================================================
Name "${APP_NAME} ${APP_VERSION}"
OutFile "${APP_NAME}-${APP_VERSION}-Setup.exe"
InstallDir "$PROGRAMFILES64\${APP_NAME}"
InstallDirRegKey HKLM "${REG_KEY}" "InstallLocation"
RequestExecutionLevel admin
ShowInstDetails show
ShowUninstDetails show

; ============================================================================
; Modern UI Configuration
; ============================================================================
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; Welcome page
!define MUI_WELCOMEPAGE_TITLE "Welcome to ${APP_NAME} Setup"
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of ${APP_NAME}.$\r$\n$\r$\n${APP_NAME} is a high-performance log and data file viewer that can handle files of any size.$\r$\n$\r$\nClick Next to continue."

; Finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "Launch ${APP_NAME}"

; ============================================================================
; Installer Pages
; ============================================================================
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Language
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "SimpChinese"

; ============================================================================
; Installer Section
; ============================================================================
Section "Install"
    SetOutPath "$INSTDIR"
    
    ; Copy all files from Release folder
    File /r "Release\*.*"
    
    ; Create Start Menu shortcuts
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
    
    ; Create Desktop shortcut (optional)
    CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
    
    ; Write uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    
    ; Write registry keys for Add/Remove Programs
    WriteRegStr HKLM "${REG_KEY}" "DisplayName" "${APP_NAME}"
    WriteRegStr HKLM "${REG_KEY}" "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKLM "${REG_KEY}" "Publisher" "${APP_PUBLISHER}"
    WriteRegStr HKLM "${REG_KEY}" "URLInfoAbout" "${APP_URL}"
    WriteRegStr HKLM "${REG_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "${REG_KEY}" "UninstallString" "$INSTDIR\Uninstall.exe"
    WriteRegStr HKLM "${REG_KEY}" "DisplayIcon" "$INSTDIR\${APP_EXE}"
    WriteRegDWORD HKLM "${REG_KEY}" "NoModify" 1
    WriteRegDWORD HKLM "${REG_KEY}" "NoRepair" 1
    
    ; Calculate installed size
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "${REG_KEY}" "EstimatedSize" "$0"
    
    ; Register file associations (optional)
    ; .log files
    WriteRegStr HKCR ".log\OpenWithProgids" "${APP_NAME}.log" ""
    WriteRegStr HKCR "${APP_NAME}.log" "" "Log File"
    WriteRegStr HKCR "${APP_NAME}.log\DefaultIcon" "" "$INSTDIR\${APP_EXE},0"
    WriteRegStr HKCR "${APP_NAME}.log\shell\open\command" "" '"$INSTDIR\${APP_EXE}" "%1"'
    
    ; .csv files
    WriteRegStr HKCR ".csv\OpenWithProgids" "${APP_NAME}.csv" ""
    WriteRegStr HKCR "${APP_NAME}.csv" "" "CSV File"
    WriteRegStr HKCR "${APP_NAME}.csv\DefaultIcon" "" "$INSTDIR\${APP_EXE},0"
    WriteRegStr HKCR "${APP_NAME}.csv\shell\open\command" "" '"$INSTDIR\${APP_EXE}" "%1"'
    
    ; Refresh shell
    System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'
SectionEnd

; ============================================================================
; Uninstaller Section
; ============================================================================
Section "Uninstall"
    ; Remove files
    RMDir /r "$INSTDIR"
    
    ; Remove Start Menu shortcuts
    RMDir /r "$SMPROGRAMS\${APP_NAME}"
    
    ; Remove Desktop shortcut
    Delete "$DESKTOP\${APP_NAME}.lnk"
    
    ; Remove registry keys
    DeleteRegKey HKLM "${REG_KEY}"
    
    ; Remove file associations
    DeleteRegKey HKCR "${APP_NAME}.log"
    DeleteRegKey HKCR "${APP_NAME}.csv"
    DeleteRegValue HKCR ".log\OpenWithProgids" "${APP_NAME}.log"
    DeleteRegValue HKCR ".csv\OpenWithProgids" "${APP_NAME}.csv"
    
    ; Refresh shell
    System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'
SectionEnd

; ============================================================================
; Version Info
; ============================================================================
VIProductVersion "${APP_VERSION}.0"
VIAddVersionKey "ProductName" "${APP_NAME}"
VIAddVersionKey "ProductVersion" "${APP_VERSION}"
VIAddVersionKey "CompanyName" "${APP_PUBLISHER}"
VIAddVersionKey "LegalCopyright" "Copyright (C) 2024-2026 ${APP_PUBLISHER}"
VIAddVersionKey "FileDescription" "${APP_NAME} Installer"
VIAddVersionKey "FileVersion" "${APP_VERSION}"
