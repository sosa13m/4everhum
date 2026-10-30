# Desinstalador de 4EVERHUM para Windows (doble clic en desinstalar.bat).
# Quita el plugin VST3, la app, el acceso directo y la entrada del PATH.
# Tus .mid y tus tomas .wav guardadas NO se borran.

param(
    [string] $VstDir    = (Join-Path ${env:CommonProgramFiles} 'VST3'),
    [string] $AppDir    = (Join-Path ${env:LOCALAPPDATA} '4EVERHUM'),
    [string] $StartMenu = (Join-Path ${env:APPDATA} 'Microsoft\Windows\Start Menu\Programs'),
    [switch] $NoElevate,
    [switch] $NoPause
)

$ErrorActionPreference = 'Stop'
$isWindowsOs = [System.Environment]::OSVersion.Platform -eq 'Win32NT'

function Finish ($code) {
    if (-not $NoPause) { Write-Host ''; Read-Host 'Pulsa Enter para cerrar' | Out-Null }
    exit $code
}

if ($isWindowsOs -and -not $NoElevate) {
    $principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        # Se copia a TEMP para poder borrar la carpeta de la app mientras corre.
        $tmp = Join-Path ${env:TEMP} '4everhum-desinstalar.ps1'
        Copy-Item -Force $MyInvocation.MyCommand.Path $tmp
        Start-Process powershell.exe -Verb RunAs -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$tmp`"",
                                                                  '-AppDir', "`"$AppDir`"", '-StartMenu', "`"$StartMenu`"")
        exit 0
    }
}

try {
    $target = Join-Path $VstDir '4EVERHUM.vst3'
    if (Test-Path $target) {
        try { Remove-Item -Recurse -Force $target }
        catch { throw 'No pude borrar el plugin. Cierra FL Studio (y cualquier DAW) y vuelve a intentar.' }
    }
    if (Test-Path $AppDir) { Remove-Item -Recurse -Force $AppDir }

    $lnk = Join-Path $StartMenu '4EVERHUM.lnk'
    if (Test-Path $lnk) { Remove-Item -Force $lnk }

    if ($isWindowsOs) {
        $userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
        if ($userPath) {
            $kept = ($userPath -split ';') | Where-Object { $_ -and ($_ -ne $AppDir) }
            [Environment]::SetEnvironmentVariable('Path', ($kept -join ';'), 'User')
        }
    }

    Write-Host '  4EVERHUM desinstalado.' -ForegroundColor Green
    Finish 0
}
catch {
    Write-Host "  Error: $($_.Exception.Message)" -ForegroundColor Red
    Finish 1
}
