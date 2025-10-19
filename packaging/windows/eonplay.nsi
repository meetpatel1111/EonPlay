; EonPlay NSIS Installer Script
; Timeless, futuristic media player - play for eons

!define PRODUCT_NAME "EonPlay"
!define PRODUCT_VERSION "1.0.0"
!define PRODUCT_PUBLISHER "EonPlay Team"
!define PRODUCT_WEB_SITE "https://eonplay.com"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\EonPlay.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

; Modern UI
!include "MUI2.nsh"
!include "FileFunc.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "resources\icons\app_icon.ico"
!define MUI_UNICON "resources\icons\app_icon.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "resources\installer\welcome.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "resources\installer\welcome.bmp"

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page
!insertmacro MUI_PAGE_LICENSE "LICENSE.txt"
; Components page
!insertmacro MUI_PAGE_COMPONENTS
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\EonPlay.exe"
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\README.txt"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

; Reserve files
!insertmacro MUI_RESERVEFILE_INSTALLOPTIONS

; MUI end ------

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "EonPlay-${PRODUCT_VERSION}-Setup.exe"
InstallDir "$PROGRAMFILES64\EonPlay"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

; Version Information
VIProductVersion "1.0.0.0"
VIAddVersionKey "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey "Comments" "Timeless, futuristic media player"
VIAddVersionKey "CompanyName" "${PRODUCT_PUBLISHER}"
VIAddVersionKey "LegalTrademarks" "EonPlay is a trademark of ${PRODUCT_PUBLISHER}"
VIAddVersionKey "LegalCopyright" "© ${PRODUCT_PUBLISHER}"
VIAddVersionKey "FileDescription" "${PRODUCT_NAME} Installer"
VIAddVersionKey "FileVersion" "${PRODUCT_VERSION}"
VIAddVersionKey "ProductVersion" "${PRODUCT_VERSION}"

; Request application privileges for Windows Vista/7/8/10/11
RequestExecutionLevel admin

Section "EonPlay Core" SEC01
  SectionIn RO
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  
  ; Main application files
  File "EonPlay.exe"
  File "*.dll"
  File "LICENSE.txt"
  File "README.txt"
  
  ; Qt libraries
  File /r "platforms\"
  File /r "imageformats\"
  File /r "audio\"
  File /r "mediaservice\"
  
  ; VLC libraries
  File /r "plugins\"
  File "libvlc.dll"
  File "libvlccore.dll"
  
  ; Resources
  SetOutPath "$INSTDIR\resources"
  File /r "resources\*"
  
  ; Create application data directory
  CreateDirectory "$APPDATA\EonPlay"
  
SectionEnd

Section "Desktop Shortcut" SEC02
  CreateShortCut "$DESKTOP\EonPlay.lnk" "$INSTDIR\EonPlay.exe"
SectionEnd

Section "Start Menu Shortcuts" SEC03
  CreateDirectory "$SMPROGRAMS\EonPlay"
  CreateShortCut "$SMPROGRAMS\EonPlay\EonPlay.lnk" "$INSTDIR\EonPlay.exe"
  CreateShortCut "$SMPROGRAMS\EonPlay\Uninstall.lnk" "$INSTDIR\uninst.exe"
SectionEnd

Section "File Associations" SEC04
  ; Register file associations for common media formats
  WriteRegStr HKCR ".mp4" "" "EonPlay.MediaFile"
  WriteRegStr HKCR ".avi" "" "EonPlay.MediaFile"
  WriteRegStr HKCR ".mkv" "" "EonPlay.MediaFile"
  WriteRegStr HKCR ".mp3" "" "EonPlay.MediaFile"
  WriteRegStr HKCR ".flac" "" "EonPlay.MediaFile"
  WriteRegStr HKCR ".wav" "" "EonPlay.MediaFile"
  WriteRegStr HKCR ".ogg" "" "EonPlay.MediaFile"
  WriteRegStr HKCR ".webm" "" "EonPlay.MediaFile"
  WriteRegStr HKCR ".mov" "" "EonPlay.MediaFile"
  
  WriteRegStr HKCR "EonPlay.MediaFile" "" "EonPlay Media File"
  WriteRegStr HKCR "EonPlay.MediaFile\DefaultIcon" "" "$INSTDIR\EonPlay.exe,0"
  WriteRegStr HKCR "EonPlay.MediaFile\shell\open\command" "" '"$INSTDIR\EonPlay.exe" "%1"'
  
  ; Notify shell of changes
  System::Call 'shell32.dll::SHChangeNotify(i, i, i, i) v (0x08000000, 0, 0, 0)'
SectionEnd

Section "Visual C++ Redistributable" SEC05
  ; Check if VC++ Redistributable is already installed
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" "Version"
  StrCmp $0 "" 0 vcredist_done
  
  ; Download and install VC++ Redistributable
  DetailPrint "Installing Visual C++ Redistributable..."
  File "vcredist_x64.exe"
  ExecWait '"$INSTDIR\vcredist_x64.exe" /quiet /norestart'
  Delete "$INSTDIR\vcredist_x64.exe"
  
  vcredist_done:
SectionEnd

Section -AdditionalIcons
  WriteIniStr "$INSTDIR\${PRODUCT_NAME}.url" "InternetShortcut" "URL" "${PRODUCT_WEB_SITE}"
  CreateShortCut "$SMPROGRAMS\EonPlay\Website.lnk" "$INSTDIR\${PRODUCT_NAME}.url"
  CreateShortCut "$SMPROGRAMS\EonPlay\Uninstall.lnk" "$INSTDIR\uninst.exe"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\EonPlay.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\EonPlay.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  
  ; Calculate installed size
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "EstimatedSize" "$0"
SectionEnd

; Section descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC01} "Core EonPlay application files (required)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC02} "Create a desktop shortcut for EonPlay"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC03} "Create Start Menu shortcuts"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC04} "Associate media file types with EonPlay"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC05} "Install Visual C++ Redistributable (required for proper operation)"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) was successfully removed from your computer."
FunctionEnd

Function un.onInit
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Are you sure you want to completely remove $(^Name) and all of its components?" IDYES +2
  Abort
FunctionEnd

Section Uninstall
  ; Remove registry keys
  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  
  ; Remove file associations
  DeleteRegKey HKCR ".mp4"
  DeleteRegKey HKCR ".avi"
  DeleteRegKey HKCR ".mkv"
  DeleteRegKey HKCR ".mp3"
  DeleteRegKey HKCR ".flac"
  DeleteRegKey HKCR ".wav"
  DeleteRegKey HKCR ".ogg"
  DeleteRegKey HKCR ".webm"
  DeleteRegKey HKCR ".mov"
  DeleteRegKey HKCR "EonPlay.MediaFile"
  
  ; Remove files and directories
  Delete "$INSTDIR\${PRODUCT_NAME}.url"
  Delete "$INSTDIR\uninst.exe"
  Delete "$INSTDIR\EonPlay.exe"
  Delete "$INSTDIR\*.dll"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\README.txt"
  
  RMDir /r "$INSTDIR\platforms"
  RMDir /r "$INSTDIR\imageformats"
  RMDir /r "$INSTDIR\audio"
  RMDir /r "$INSTDIR\mediaservice"
  RMDir /r "$INSTDIR\plugins"
  RMDir /r "$INSTDIR\resources"
  
  ; Remove shortcuts
  Delete "$DESKTOP\EonPlay.lnk"
  Delete "$SMPROGRAMS\EonPlay\*"
  RMDir "$SMPROGRAMS\EonPlay"
  
  ; Remove installation directory if empty
  RMDir "$INSTDIR"
  
  ; Ask if user wants to remove user data
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Do you want to remove your EonPlay user data and settings?" IDNO +3
  RMDir /r "$APPDATA\EonPlay"
  RMDir /r "$LOCALAPPDATA\EonPlay"
  
  ; Notify shell of changes
  System::Call 'shell32.dll::SHChangeNotify(i, i, i, i) v (0x08000000, 0, 0, 0)'
  
  SetAutoClose true
SectionEnd