# Downloads the prebuilt Qt that favdir builds against. It goes into the folder named by the
# QT_ROOT environment variable, for a Qt folder that several projects share, or else into .qt/
# at the repository root. Requires uv (https://docs.astral.sh/uv/).

$ErrorActionPreference = 'Stop'

$QtVersion = '6.8.3'
$QtArch = 'win64_msvc2022_64'
$QtArchDir = 'msvc2022_64'

$Root = Split-Path -Parent $PSScriptRoot
$QtRoot = if ($env:QT_ROOT) { $env:QT_ROOT } else { Join-Path $Root '.qt' }
$QtDir = Join-Path $QtRoot "$QtVersion\$QtArchDir"

if (Test-Path (Join-Path $QtDir 'lib\cmake\Qt6')) {
    Write-Host "Qt $QtVersion already installed at $QtDir"
    exit 0
}

if (-not (Get-Command uvx -ErrorAction SilentlyContinue)) {
    Write-Error 'uv is required. Install it from https://docs.astral.sh/uv/ and run this script again.'
}

& uvx --from aqtinstall aqt install-qt windows desktop $QtVersion $QtArch -O $QtRoot --archives qtbase
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Qt $QtVersion installed at $QtDir"
