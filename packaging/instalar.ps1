# Instalador de 4EVERHUM para Windows.
#
# Doble clic en instalar.bat (o: powershell -ExecutionPolicy Bypass -File instalar.ps1)
#
#  - Copia el plugin 4EVERHUM.vst3 a la carpeta estandar de VST3 (la que escanea FL Studio)
#  - Instala la app Standalone y hum2midi en %LOCALAPPDATA%\4EVERHUM
#  - Crea un acceso directo "4EVERHUM" en el menu Inicio
#  - Agrega hum2midi al PATH de tu usuario
#
# Pide permisos de administrador una sola vez (para escribir en Archivos de programa).

param(
    [string] $VstDir    = (Join-Path ${env:CommonProgramFiles} 'VST3'),
    [string] $AppDir    = (Join-Path ${env:LOCALAPPDATA} '4EVERHUM'),
    [string] $StartMenu = (Join-Path ${env:APPDATA} 'Microsoft\Windows\Start Menu\Programs'),
    [switch] $NoElevate,   # para pruebas: no pedir administrador
    [switch] $NoPath,      # no tocar el PATH
    [switch] $NoPause      # no esperar una tecla al terminar
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path

function Write-Step ($text) { Write-Host "  > $text" -ForegroundColor Gray }
function Finish ($code) {
    if (-not $NoPause) { Write-Host ''; Read-Host 'Pulsa Enter para cerrar' | Out-Null }
    exit $code
}

# --- Administrador (solo en Windows y solo si hace falta) ---------------------------
$isWindowsOs = [System.Environment]::OSVersion.Platform -eq 'Win32NT'
if ($isWindowsOs -and -not $NoElevate) {
    $principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        Write-Host 'Pidiendo permisos de administrador para copiar el plugin...' -ForegroundColor Yellow
        $argsList = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$($MyInvocation.MyCommand.Path)`"",
                      '-AppDir', "`"$AppDir`"", '-StartMenu', "`"$StartMenu`"")
        if ($NoPath) { $argsList += '-NoPath' }
        Start-Process powershell.exe -Verb RunAs -ArgumentList $argsList
        exit 0
    }
}

Write-Host ''
Write-Host '  4EVERHUM - tararea -> MIDI' -ForegroundColor Red
Write-Host ''

try {
    # --- Archivos del paquete -------------------------------------------------------
    $vst = Get-ChildItem -Path $here -Recurse -Directory -Filter '4EVERHUM.vst3' | Select-Object -First 1
    if (-not $vst) { throw 'No encontre 4EVERHUM.vst3 junto al instalador. Descomprime el zip completo y vuelve a intentar.' }
    $standalone = Get-ChildItem -Path $here -Recurse -File -Filter '4EVERHUM.exe' | Select-Object -First 1
    $hum2midi   = Get-ChildItem -Path $here -Recurse -File -Filter 'hum2midi.exe' | Select-Object -First 1

    # --- Plugin VST3 ------------------------------------------------------------------
    Write-Step "Plugin VST3 -> $VstDir"
    New-Item -ItemType Directory -Force -Path $VstDir | Out-Null
    $target = Join-Path $VstDir '4EVERHUM.vst3'
    if (Test-Path $target) {
        try { Remove-Item -Recurse -Force $target }
        catch { throw 'No pude reemplazar el 4EVERHUM.vst3 anterior. Cierra FL Studio (y cualquier DAW) y vuelve a intentar.' }
    }
    Copy-Item -Recurse -Force $vst.FullName $target

    # --- App Standalone + hum2midi ------------------------------------------------------
    Write-Step "App y hum2midi -> $AppDir"
    New-Item -ItemType Directory -Force -Path $AppDir | Out-Null
    if ($standalone) { Copy-Item -Force $standalone.FullName (Join-Path $AppDir '4EVERHUM.exe') }
    if ($hum2midi)   { Copy-Item -Force $hum2midi.FullName   (Join-Path $AppDir 'hum2midi.exe') }
    foreach ($doc in @('LEEME.txt', 'desinstalar.ps1', 'desinstalar.bat', 'LICENSE', 'TERCEROS.md')) {
        $src = Join-Path $here $doc
        if (Test-Path $src) { Copy-Item -Force $src (Join-Path $AppDir $doc) }
    }

    # Archivos bajados de internet: quitar la marca para que Windows no los bloquee.
    if ($isWindowsOs) {
        Get-ChildItem -Recurse $target, $AppDir -File | ForEach-Object { Unblock-File -Path $_.FullName -ErrorAction SilentlyContinue }
    }

    # --- Acceso directo -----------------------------------------------------------------
    if ($isWindowsOs -and $standalone) {
        Write-Step 'Acceso directo en el menu Inicio'
        New-Item -ItemType Directory -Force -Path $StartMenu | Out-Null
        $shell = New-Object -ComObject WScript.Shell
        $lnk = $shell.CreateShortcut((Join-Path $StartMenu '4EVERHUM.lnk'))
        $lnk.TargetPath = Join-Path $AppDir '4EVERHUM.exe'
        $lnk.WorkingDirectory = $AppDir
        $lnk.Description = '4EVERHUM - tararea -> MIDI'
        $lnk.Save()
    }

    # --- PATH del usuario (para usar hum2midi desde cualquier terminal) -----------------
    if ($isWindowsOs -and -not $NoPath -and $hum2midi) {
        $userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
        if (-not $userPath) { $userPath = '' }
        if (($userPath -split ';') -notcontains $AppDir) {
            Write-Step 'hum2midi agregado a tu PATH (abre una terminal nueva para usarlo)'
            [Environment]::SetEnvironmentVariable('Path', ($userPath.TrimEnd(';') + ';' + $AppDir).TrimStart(';'), 'User')
        }
    }

    Write-Host ''
    Write-Host '  Listo.' -ForegroundColor Green
    Write-Host '  En FL Studio: Options > Manage plugins > Find plugins, y busca 4EVERHUM.'
    Write-Host '  La app sin DAW esta en el menu Inicio como "4EVERHUM".'
    Finish 0
}
catch {
    Write-Host ''
    Write-Host "  Error: $($_.Exception.Message)" -ForegroundColor Red
    Finish 1
}
