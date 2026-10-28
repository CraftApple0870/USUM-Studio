<#
.SYNOPSIS
    Cree un paquet portable Windows x64 depuis une compilation EXISTANTE de USUM Studio.
.DESCRIPTION
    - Ne recompile rien et ne modifie aucun fichier source.
    - Installe dans un dossier temporaire unique via CMake.
    - Verifie que les executables et DLL inclus sont bien en x64 (PE32+ / AMD64).
    - Verifie la presence de SDL3.dll et sa dependance depuis usum-viewport.exe.
    - Inclut les resources, shaders et, si disponible, le compilateur Pawn.
    - Cree un ZIP et un fichier de sommes SHA-256; ne publie rien sur GitHub.
.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File .\package-usum-portable-x64.ps1
.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File .\package-usum-portable-x64.ps1 -AllowMissingPawn
#>
[CmdletBinding()]
param(
    [string]$BuildDir = 'build-clean-x64',
    [string]$OutputDir = 'dist',
    [switch]$AllowMissingPawn
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $root

function Assert-True([bool]$condition, [string]$message) {
    if (-not $condition) { throw $message }
}
function Get-FullPath([string]$relative) {
    return [System.IO.Path]::GetFullPath((Join-Path $root $relative))
}
function Assert-PeX64([string]$file) {
    Assert-True (Test-Path -LiteralPath $file -PathType Leaf) "Fichier introuvable : $file"
    $stream = [System.IO.File]::OpenRead($file)
    try {
        $reader = New-Object System.IO.BinaryReader($stream)
        Assert-True ($stream.Length -ge 0x40) "Fichier PE trop court : $file"
        Assert-True ($reader.ReadUInt16() -eq 0x5A4D) "Signature MZ absente : $file"
        $stream.Position = 0x3C
        $offset = $reader.ReadInt32()
        Assert-True ($offset -ge 0x40 -and ($offset + 26) -le $stream.Length) "En-tete PE invalide : $file"
        $stream.Position = $offset
        Assert-True ($reader.ReadUInt32() -eq 0x00004550) "Signature PE absente : $file"
        $machine = $reader.ReadUInt16()
        Assert-True ($machine -eq 0x8664) "Architecture incorrecte dans $file (Machine=0x$($machine.ToString('X4')), x64 attendu)."
        $stream.Position = $offset + 24
        $magic = $reader.ReadUInt16()
        Assert-True ($magic -eq 0x20B) "Format PE32+ (64 bits) attendu : $file"
    }
    finally { $stream.Dispose() }
}
function Copy-FileChecked([string]$source, [string]$destination) {
    Assert-True (Test-Path -LiteralPath $source -PathType Leaf) "Fichier requis introuvable : $source"
    $directory = Split-Path -Parent $destination
    if (-not (Test-Path -LiteralPath $directory)) { New-Item -ItemType Directory -Path $directory -Force | Out-Null }
    Copy-Item -LiteralPath $source -Destination $destination -Force
}

$build = Get-FullPath $BuildDir
$dist = Get-FullPath $OutputDir
$cache = Join-Path $build 'CMakeCache.txt'
Assert-True (Test-Path -LiteralPath $cache -PathType Leaf) "Aucun cache CMake dans $build. Compile d'abord USUM Studio en x64 Release."
$cacheText = [System.IO.File]::ReadAllText($cache)
Assert-True ($cacheText -match '(?m)^CMAKE_BUILD_TYPE:STRING=Release\s*$') 'Le cache CMake n indique pas Release.'
Assert-True ($cacheText -match '(?mi)^CMAKE_CXX_COMPILER:FILEPATH=.*[/\\]x64[/\\]cl\.exe\s*$') 'Le compilateur du cache ne cible pas x64.'
Assert-True ($cacheText -match '(?m)^CMAKE_GENERATOR:INTERNAL=Ninja\s*$') 'Generateur Ninja attendu pour ce dossier de build.'
Assert-True (-not ($cacheText -match '(?m)^STUDIO_SDL_SOURCE:[^=]*=(?!\s*$)[^\r\n]+')) 'SDL3 doit etre chargee en DLL (STUDIO_SDL_SOURCE doit etre vide).'

$exe = Join-Path $build 'bin\usum-viewport.exe'
$dll = Join-Path $build 'bin\SDL3.dll'
Assert-PeX64 $exe
Assert-PeX64 $dll

# dumpbin confirme la dependance DYNAMIQUE au DLL (pas seulement sa presence dans le ZIP).
$dumpbin = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
Assert-True ($null -ne $dumpbin) 'dumpbin.exe introuvable. Ouvre Developer PowerShell x64 de Visual Studio puis relance.'
$imports = (& $dumpbin.Source /dependents $exe 2>&1 | Out-String)
Assert-True ($LASTEXITCODE -eq 0) 'dumpbin /dependents a echoue.'
Assert-True ($imports -match '(?im)^\s*SDL3\.dll\s*$') 'Le nouvel EXE ne depend pas de SDL3.dll : compilation SDL statique ou configuration incorrecte.'

if (-not (Test-Path -LiteralPath $dist)) { New-Item -ItemType Directory -Path $dist -Force | Out-Null }
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$stage = Join-Path $dist (".stage-usum-x64-" + [guid]::NewGuid().ToString('N'))
$zipPath = Join-Path $dist ("USUMStudio-Portable-x64-" + $stamp + '.zip')
$hashPath = $zipPath + '.sha256.txt'
Assert-True (-not (Test-Path -LiteralPath $zipPath)) "L archive existe deja : $zipPath"
New-Item -ItemType Directory -Path $stage | Out-Null

try {
    Write-Host '[1/5] Installation CMake (sans recompilation)...' -ForegroundColor Cyan
    & cmake --install $build --prefix $stage --config Release
    Assert-True ($LASTEXITCODE -eq 0) 'cmake --install a echoue.'

    $packagedExe = Join-Path $stage 'usum-viewport.exe'
    Assert-PeX64 $packagedExe
    Assert-True (Test-Path -LiteralPath (Join-Path $stage 'resources') -PathType Container) 'Dossier resources manquant.'
    Assert-True (Test-Path -LiteralPath (Join-Path $stage 'viewport-shaders') -PathType Container) 'Dossier viewport-shaders manquant.'

    Write-Host '[2/5] Ajout de SDL3 et controle des runtimes...' -ForegroundColor Cyan
    Copy-FileChecked $dll (Join-Path $stage 'SDL3.dll')
    $vcDlls = @(Get-ChildItem -LiteralPath $stage -File -Filter '*.dll')
    foreach ($file in $vcDlls) { Assert-PeX64 $file.FullName }
    # Le package peut utiliser les DLL redistribuables Microsoft ou demander l'installation du redist.
    $runtimeDll = @('vcruntime140.dll', 'msvcp140.dll')
    $missingRuntime = @($runtimeDll | Where-Object { -not (Test-Path -LiteralPath (Join-Path $stage $_)) })
    if ($missingRuntime.Count -gt 0) {
        throw "DLL runtime Visual C++ absentes du paquet : $($missingRuntime -join ', '). Installe le workload MSVC x64 et verifie InstallRequiredSystemLibraries dans CMake."
    }

    Write-Host '[3/5] Verification du compilateur Pawn...' -ForegroundColor Cyan
    $pawnLocations = @(
        (Join-Path $root 'build\pawn\bin\gf-pawncc.exe'),
        (Join-Path $build 'pawn\bin\gf-pawncc.exe'),
        (Join-Path $root 'build-pawn-x64\bin\gf-pawncc.exe')
    )
    $pawn = $pawnLocations | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    if ($null -eq $pawn) {
        if (-not $AllowMissingPawn) {
            throw 'gf-pawncc.exe introuvable. Compile le Pawn compiler ou relance avec -AllowMissingPawn (fonction de compilation Pawn indisponible).'
        }
        Write-Warning 'Le compilateur Pawn est absent. Les fonctions de compilation de scripts Pawn ne seront pas disponibles.'
    }
    else {
        Assert-PeX64 $pawn
        Copy-FileChecked $pawn (Join-Path $stage 'resources\pawn\gf-pawncc.exe')
    }

    # Rendre explicite la provenance et les limitations de cette build de test.
    $notes = @'
USUM STUDIO - WINDOWS X64 TEST BUILD

This is an unverified test build. Do not redistribute until security review is complete.
Extract the entire ZIP to a normal folder before starting usum-viewport.exe.
Keep SDL3.dll, resources, and viewport-shaders next to the executable.
The program was built for x64 Windows with SDL3 as a separate DLL.
A clean VirusTotal score alone is not proof of security.
'@
    [System.IO.File]::WriteAllText((Join-Path $stage 'README-TEST-BUILD.txt'), $notes, [System.Text.UTF8Encoding]::new($false))

    Write-Host '[4/5] Controle final des binaires du paquet...' -ForegroundColor Cyan
    foreach ($file in Get-ChildItem -LiteralPath $stage -File -Recurse | Where-Object { $_.Extension -in @('.exe', '.dll') }) {
        Assert-PeX64 $file.FullName
    }
    Write-Host '[5/5] Creation du ZIP et de son empreinte SHA-256...' -ForegroundColor Cyan
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zipPath -CompressionLevel Optimal
    Assert-True (Test-Path -LiteralPath $zipPath -PathType Leaf) 'Creation du ZIP echouee.'
    $hash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
    [System.IO.File]::WriteAllText($hashPath, "$hash  $(Split-Path -Leaf $zipPath)`r`n", [System.Text.Encoding]::ASCII)
    Write-Host "ZIP de test : $zipPath" -ForegroundColor Green
    Write-Host "SHA-256     : $hash" -ForegroundColor Green
    Write-Host "Manifeste   : $hashPath" -ForegroundColor Green
    Write-Host 'Aucune publication GitHub effectuee.' -ForegroundColor Yellow
}
finally {
    # Seul le dossier temporaire UNIQUE cree par ce script peut etre efface.
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
}
