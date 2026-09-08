param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$p07Root = Split-Path -Parent $PSScriptRoot
$p07Output = [System.IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $p07Output) { throw 'Choose a fresh viewer capture directory.' }
[void](New-Item -ItemType Directory -Path $p07Output)
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class CharacterViewerDesktop {
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int X,Y; }
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out Rect r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref Point p);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int command);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int w,int z,uint flags);
    [DllImport("user32.dll")] public static extern void keybd_event(byte key,byte scan,uint flags,UIntPtr e);
    [DllImport("user32.dll", EntryPoint="PostMessageW")] public static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
}
'@
$p07Process = Start-Process -FilePath (Join-Path $p07Root 'build/debug/bin/character_animation_viewer.exe') -WorkingDirectory $p07Output -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $p07Output 'viewer.log') -RedirectStandardError (Join-Path $p07Output 'viewer-errors.log')
$p07NativeHandle = $p07Process.Handle # Retain the native handle for exit-code readback.
try {
    for ($p07Attempt = 0; $p07Attempt -lt 100; ++$p07Attempt) {
        $p07Process.Refresh()
        if ($p07Process.HasExited) { throw 'Viewer exited during startup.' }
        if ($p07Process.MainWindowHandle -ne 0) { break }
        Start-Sleep -Milliseconds 100
    }
    $p07Handle = $p07Process.MainWindowHandle
    if ($p07Handle -eq 0) { throw 'Viewer window did not appear.' }
    [void][CharacterViewerDesktop]::ShowWindow($p07Handle,9)
    [void][CharacterViewerDesktop]::SetWindowPos($p07Handle,[IntPtr](-1),20,20,0,0,1)
    [void][CharacterViewerDesktop]::SetForegroundWindow($p07Handle)
    Start-Sleep -Milliseconds 800
    function Press-ViewerKey([byte]$Key) {
        [CharacterViewerDesktop]::keybd_event($Key,0,0,[UIntPtr]::Zero)
        Start-Sleep -Milliseconds 90
        [CharacterViewerDesktop]::keybd_event($Key,0,2,[UIntPtr]::Zero)
        Start-Sleep -Milliseconds 180
    }
    function Save-ViewerCapture([string]$Name) {
        $p07Process.Refresh()
        if ($p07Process.HasExited) { throw 'Viewer exited; inspect viewer-errors.log.' }
        $p07Origin = New-Object CharacterViewerDesktop+Point
        $p07Rect = New-Object CharacterViewerDesktop+Rect
        [void][CharacterViewerDesktop]::ClientToScreen($p07Handle,[ref]$p07Origin)
        if (![CharacterViewerDesktop]::GetClientRect($p07Handle,[ref]$p07Rect) -or $p07Rect.Right -le 0 -or $p07Rect.Bottom -le 0) { throw 'Viewer client rectangle is unavailable.' }
        $p07Bitmap = New-Object System.Drawing.Bitmap($p07Rect.Right,$p07Rect.Bottom)
        $p07Graphics = [System.Drawing.Graphics]::FromImage($p07Bitmap)
        try {
            $p07Graphics.CopyFromScreen($p07Origin.X,$p07Origin.Y,0,0,$p07Bitmap.Size)
            $p07Bitmap.Save((Join-Path $p07Output ($Name + '.png')),[System.Drawing.Imaging.ImageFormat]::Png)
        } finally { $p07Graphics.Dispose(); $p07Bitmap.Dispose() }
    }
    Press-ViewerKey 80 # pause
    Save-ViewerCapture 'idle-paused'
    Press-ViewerKey 116 # F5 walk
    Press-ViewerKey 68 # seek +0.1; seek exits transition and pauses
    Save-ViewerCapture 'walk-seek'
    Start-Sleep -Milliseconds 400
    Save-ViewerCapture 'walk-still-paused'
    Press-ViewerKey 32 # restart
    Save-ViewerCapture 'walk-restarted'
    Press-ViewerKey 116 # interact
    for ($p07Seek = 0; $p07Seek -lt 7; ++$p07Seek) { Press-ViewerKey 68 }
    Save-ViewerCapture 'interact-reach'
    Press-ViewerKey 77 # four
    Start-Sleep -Milliseconds 800
    Save-ViewerCapture 'four-paused'
    Press-ViewerKey 69 # side
    Press-ViewerKey 82 # flashlight
    Save-ViewerCapture 'four-side-flashlight'
    Press-ViewerKey 77 # one
    Start-Sleep -Milliseconds 800
    Press-ViewerKey 80 # resume
    Start-Sleep -Milliseconds 500
    Save-ViewerCapture 'one-resumed'
    Press-ViewerKey 27 # close
    if (!$p07Process.WaitForExit(10000)) { throw 'Viewer did not close after Escape.' }
    $p07Process.Refresh()
    if ($p07Process.ExitCode -ne 0) { throw ('Viewer returned a failure status: ' + $p07Process.ExitCode) }
    Write-Output 'Viewer keyboard sequence completed. Inspect captures and validation log.'
} finally {
    if (!$p07Process.HasExited) {
        [void][CharacterViewerDesktop]::PostMessage($p07Process.MainWindowHandle,16,[IntPtr]::Zero,[IntPtr]::Zero)
        [void]$p07Process.WaitForExit(10000)
    }
    $p07Process.Dispose()
}
