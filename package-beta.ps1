param(
  [string]$BuildDir = 'build',
  [string]$OutputDir = 'USUMStudio-Portable',
  [string]$ZipName = 'USUMStudio-Portable.zip'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root
$CMakeExe = "C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

& $CMakeExe --build $BuildDir --target usum-viewport --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
$buildPath = (Resolve-Path $BuildDir).Path
$outPath = Join-Path $root $OutputDir
if (Test-Path $outPath) { Remove-Item $outPath -Recurse -Force }
& $CMakeExe --install $buildPath --prefix $outPath
if ($LASTEXITCODE -ne 0) { throw 'Install/package failed.' }
$required = @('usum-viewport.exe','MSVCP140.dll','VCRUNTIME140.dll','resources','viewport-shaders')
$missing = @($required | Where-Object { -not (Test-Path (Join-Path $outPath $_)) })
if ($missing.Count -gt 0) {
  throw ('Incomplete portable package; missing: ' + ($missing -join ', ') + '. Verify MSVC x64 toolchain and CMake runtime discovery. No zip created.')
}
$zipPath = Join-Path $root $ZipName
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path (Join-Path $outPath '*') -DestinationPath $zipPath -CompressionLevel Optimal
Write-Host "Portable package ready: $zipPath"
