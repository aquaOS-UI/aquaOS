@echo off
setlocal
set "PROJECT_ROOT=%~dp0.."
cd /d "%PROJECT_ROOT%"

echo Restoring Windows Taskbar and Work Area...
taskkill /F /IM AquaShell.exe >nul 2>&1

powershell -NoProfile -ExecutionPolicy Bypass -Command "
Add-Type @'
using System;
using System.Runtime.InteropServices;
public class ShellHelper {
    [DllImport(\"user32.dll\")] public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
    [DllImport(\"user32.dll\")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport(\"user32.dll\")] public static extern bool SystemParametersInfo(uint uiAction, uint uiParam, IntPtr pvParam, uint fWinIni);
    public static void Restore() {
        IntPtr h1 = FindWindow(\"Shell_TrayWnd\", null);
        if (h1 != IntPtr.Zero) ShowWindow(h1, 5);
        IntPtr h2 = FindWindow(\"Shell_SecondaryTrayWnd\", null);
        if (h2 != IntPtr.Zero) ShowWindow(h2, 5);
    }
}
'@
[ShellHelper]::Restore()
" >nul 2>&1

echo Taskbar and system desktop restored successfully!
