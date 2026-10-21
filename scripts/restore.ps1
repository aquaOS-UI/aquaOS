$signature = @"
using System;
using System.Runtime.InteropServices;
public class NativeShell {
    [DllImport("user32.dll")]
    public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
}
"@
if (-not ([System.Management.Automation.PSTypeName]'NativeShell').Type) {
    Add-Type -TypeDefinition $signature -Language CSharp | Out-Null
}
$hTray = [NativeShell]::FindWindow("Shell_TrayWnd", $null)
if ($hTray -ne [IntPtr]::Zero) {
    [NativeShell]::ShowWindow($hTray, 5) | Out-Null
}
$hSec = [NativeShell]::FindWindow("Shell_SecondaryTrayWnd", $null)
if ($hSec -ne [IntPtr]::Zero) {
    [NativeShell]::ShowWindow($hSec, 5) | Out-Null
}

