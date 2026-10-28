<#
 USUM Studio - construction Windows x64 reproductible (candidat de test).
 Utilisation depuis "x64 Native Tools Command Prompt for VS" :
   powershell -NoProfile -ExecutionPolicy Bypass -File .\build-usum-clean-x64.ps1

 Ne modifie pas le code source, ne publie pas de release et n'exécute pas le binaire.
 Les archives ne doivent pas être diffusées avant la validation de sécurité.
#>
[CmdletBinding()]
param(
    [ValidateRange(1, 32)][int]$Jobs = 4,
    [string]$BuildDirName = 'build-clean-x64'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $root

function Invoke-Checked {
    param([string]$Label, [string]$Exe, [string[]]$Arguments)
    Write-Host "`n== $Label ==" -ForegroundColor Cyan
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Label failed (exit code $LASTEXITCODE)." }
}
function Assert-Exists {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { throw "Missing required file or directory: $Path" }
}
function Assert-Pe64 {
    param([string]$Path)
    Assert-Exists $Path
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $reader = [System.IO.BinaryReader]::new($stream)
        $stream.Position = 0
        if ($reader.ReadUInt16() -ne 0x5A4D) { throw "Not a PE (MZ) file: $Path" }
        $stream.Position = 0x3C
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 0x40 -or $peOffset -gt ($stream.Length - 24)) { throw "Invalid PE header: $Path" }
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { throw "Invalid PE signature: $Path" }
        $machine = $reader.ReadUInt16()
        if ($machine -ne 0x8664) { throw "Expected AMD64 PE, found machine code 0x$('{0:X4}' -f $machine): $Path" }
        Write-Host "Verified AMD64: $Path" -ForegroundColor Green
    } finally { $stream.Dispose() }
}

Assert-Exists (Join-Path $root 'CMakeLists.txt')
Assert-Exists (Join-Path $root 'cmake/dependencies.json')
Assert-Exists (Join-Path $root '.github/package.py')
foreach ($name in @('cmake','ninja','python','cl')) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
        throw "'$name' not found. Open x64 Native Tools Command Prompt and ensure CMake, Ninja, Python and MSVC are installed."
    }
}
# Le test de la variable d'environnement et du PE empêche un build x86 silencieux.
if ($env:VSCMD_ARG_TGT_ARCH -ne 'x64') {
    throw 'Run this script from x64 Native Tools Command Prompt for Visual Studio (target architecture must be x64).'
}
Invoke-Checked 'CMake version' 'cmake' @('--version')
Invoke-Checked 'Ninja version' 'ninja' @('--version')
Invoke-Checked 'Python version' 'python' @('--version')

# Utiliser un dossier neuf pour éviter le CMakeCache.txt x86 de l'ancienne installation.
if ($BuildDirName -notmatch '^build[-a-zA-Z0-9_]*$') {
    throw 'BuildDirName must be a simple name starting with build (no paths).'
}
$build = Join-Path $root $BuildDirName
if (Test-Path -LiteralPath $build) {
    throw "Build folder already exists: $build. Rename/archive it first; this script never deletes it."
}

# Téléchargements HTTPS contrôlés par les SHA-256 du manifeste CMake.
Invoke-Checked 'Download pinned dependencies' 'cmake' @('-P', 'cmake/DownloadDependencies.cmake')
$dep = Join-Path $root 'dependencies'
$graphicsSource = Join-Path $dep 'source'
$graphicsBuild = Join-Path $dep 'graphics-clean-x64'
$imgui = Join-Path $dep 'imgui'
$sdlConfig = Join-Path $dep 'SDL3-3.4.16/cmake'
$pawnSource = Join-Path $dep 'pawn-compiler'
foreach ($p in @($graphicsSource, $imgui, $sdlConfig, $pawnSource)) { Assert-Exists $p }
if (Test-Path -LiteralPath $graphicsBuild) {
    throw "Graphics build folder already exists: $graphicsBuild. Rename/archive it to avoid reusing another architecture."
}

# Compiler bgfx, bimg, bx et shaderc dans le même environnement x64.
Invoke-Checked 'Configure graphics x64' 'cmake' @('-S', $graphicsSource, '-B', $graphicsBuild, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DBGFX_BUILD_EXAMPLES=OFF', '-DBGFX_WITH_WAYLAND=OFF', '-DBGFX_AMALGAMATED=ON')
Invoke-Checked 'Build graphics x64' 'cmake' @('--build', $graphicsBuild, '--target', 'bgfx', 'bimg', 'bx', 'shaderc', '--parallel', "$Jobs")

# Le compilateur Pawn est une dépendance de la distribution actuelle.
$pawnBuild = Join-Path $build 'pawn'
Invoke-Checked 'Configure Pawn compiler x64' 'cmake' @('-S', (Join-Path $root 'cmake/pawn-compiler'), '-B', $pawnBuild, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', "-DPAWN_SOURCE=$pawnSource")
Invoke-Checked 'Build Pawn compiler x64' 'cmake' @('--build', $pawnBuild, '--parallel', "$Jobs")
$pawnExe = Join-Path $pawnBuild 'bin/gf-pawncc.exe'
Assert-Pe64 $pawnExe
Invoke-Checked 'Verify Pawn compiler' 'python' @('tools/pawn-release/verify.py', '--compiler', $pawnExe, '--include', 'resources/pawn')

# STUDIO_SDL_SOURCE volontairement absent : SDL3 est importée depuis le package VC DLL.
# SDL3_DIR doit pointer vers SDL3Config.cmake du SDK SDL3 VC, non vers SDL source.
Invoke-Checked 'Configure editor x64 Release with SDL3 DLL' 'cmake' @(
    '-S', $root, '-B', $build, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release',
    "-DSTUDIO_GRAPHICS_SOURCE=$graphicsSource",
    "-DSTUDIO_GRAPHICS_BUILD=$graphicsBuild",
    "-DSTUDIO_IMGUI_SOURCE=$imgui",
    "-DSDL3_DIR=$sdlConfig",
    '-DSTUDIO_SDL_SOURCE=',
    '-DSTUDIO_PORTABLE_DISTRIBUTION=ON'
)
Invoke-Checked 'Build editor x64 Release' 'cmake' @('--build', $build, '--target', 'usum-viewport', '--parallel', "$Jobs")
$exe = Join-Path $build 'bin/usum-viewport.exe'
$dll = Join-Path $build 'bin/SDL3.dll'
Assert-Pe64 $exe
Assert-Pe64 $dll

# Le paquet est construit par le packager du dépôt, sans modifier l'EXE.
Invoke-Checked 'Package editor and third-party notices' 'python' @('.github/package.py', 'editor', '--platform', 'windows-x64', '--build', $build, '--dependencies', $dep)
$zip = Join-Path $root 'dist/USUMStudio-windows-x64.zip'
Assert-Exists $zip

# Empreintes conservées afin de comparer les versions et les rapports antivirus.
$hashes = @($exe, $dll, $pawnExe, $zip) | ForEach-Object {
    $item = Get-FileHash -LiteralPath $_ -Algorithm SHA256
    [pscustomobject]@{ File = $item.Path; SHA256 = $item.Hash }
}
$hashFile = Join-Path $root 'dist/USUMStudio-windows-x64-SHA256.txt'
$hashes | Format-Table -AutoSize | Out-String -Width 300 | Set-Content -LiteralPath $hashFile -Encoding UTF8
$hashes | Format-Table -AutoSize
Write-Host "`nBUILD SUCCEEDED - TEST PACKAGE ONLY: $zip" -ForegroundColor Green
Write-Host 'Do not publish until the binary and bundled files have been independently security-checked.' -ForegroundColor Yellow
