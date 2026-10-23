!define APP_NAME "ChromaZ"
!ifndef APP_VERSION
  !define APP_VERSION "1.0"
!endif
!define PUBLISHER "Pawel Zayakin"
!define EXE_NAME "ChromaZ.exe"
!define ICON_NAME "app.ico"

Name "${APP_NAME}"
OutFile "..\ChromaZ_Setup.exe"
InstallDir "$PROGRAMFILES64\${APP_NAME}"
InstallDirRegKey HKLM "Software\${APP_NAME}" "Install_Dir"
RequestExecutionLevel admin

; Задаем иконку для файла инсталлятора и деинсталлятора
Icon "${ICON_NAME}"
UninstallIcon "${ICON_NAME}"

Function .onInit
    SetRegView 64
FunctionEnd

Function un.onInit
    SetRegView 64
FunctionEnd

; Пользователь может выбрать любую папку. Чтобы деинсталлятор (RMDir /r) никогда не удалил чужие файлы,
; программа всегда ставится в папку с именем ChromaZ: если выбранная папка так не называется,
; добавляем подпапку.
Function dirLeave
    StrLen $0 "\${APP_NAME}"
    StrCpy $1 $INSTDIR "" -$0
    StrCmp $1 "\${APP_NAME}" +2 0
        StrCpy $INSTDIR "$INSTDIR\${APP_NAME}"
FunctionEnd

Page directory "" "" dirLeave
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section "MainSection" SEC01
    SetOutPath "$INSTDIR"
    
    ; Копируем файлы приложения
    File /r "..\win\*.*"
    
    ; Копируем иконку приложения
    File "${ICON_NAME}"
    
    ; Запись в реестр для отмены установки (Добавление в "Программы и компоненты")
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayName" "${APP_NAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "Publisher" "${PUBLISHER}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayIcon" "$INSTDIR\${ICON_NAME}"
    
    WriteUninstaller "$INSTDIR\uninstall.exe"
    
    ; Создание ярлыков в Пуске и на Рабочем столе с привязанной иконкой
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${EXE_NAME}" "" "$INSTDIR\${ICON_NAME}" 0
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\${ICON_NAME}" 0
    CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${EXE_NAME}" "" "$INSTDIR\${ICON_NAME}" 0
SectionEnd

Section "Uninstall"
    ; Удаление файлов и ярлыков
    Delete "$DESKTOP\${APP_NAME}.lnk"
    Delete "$SMPROGRAMS\${APP_NAME}\*.*"
    RMDir "$SMPROGRAMS\${APP_NAME}"
    
    StrLen $0 "\${APP_NAME}"
    StrCpy $1 $INSTDIR "" -$0
    StrCmp $1 "\${APP_NAME}" removeAll removeKnown
removeAll:
    Delete "$INSTDIR\${ICON_NAME}"
    RMDir /r "$INSTDIR"
    Goto removeDone
removeKnown:
    ; Папка установки называется иначе: удаляем только файлы самой программы, а не всю папку.
    Delete "$INSTDIR\${EXE_NAME}"
    Delete "$INSTDIR\${ICON_NAME}"
    Delete "$INSTDIR\uninstall.exe"
    RMDir "$INSTDIR"
removeDone:
    
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
    DeleteRegKey HKLM "Software\${APP_NAME}"
SectionEnd