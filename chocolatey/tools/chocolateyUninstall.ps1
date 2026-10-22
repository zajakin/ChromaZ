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
} else {
  Write-Warning "Uninstaller not found at $uninstaller"
}
