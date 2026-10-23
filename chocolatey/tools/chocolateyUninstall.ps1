$ErrorActionPreference = 'Stop'

$installDir = "$env:ProgramFiles\ChromaZ"
$uninstaller = Join-Path $installDir "uninstall.exe"

if (Test-Path $uninstaller) {
  $packageArgs = @{
    packageName = 'chromaz'
    fileType    = 'exe'
    silentArgs  = "/S _?=$installDir"
    file        = $uninstaller
  }
  Uninstall-ChocolateyPackage @packageArgs

  # NSIS при запуске с _?= выполняется на месте и не удаляет ни сам uninstall.exe, ни папку установки.
  if (Test-Path $installDir) {
    Remove-Item -Path $installDir -Recurse -Force -ErrorAction SilentlyContinue
  }
} else {
  Write-Warning "Uninstaller not found at $uninstaller"
}
