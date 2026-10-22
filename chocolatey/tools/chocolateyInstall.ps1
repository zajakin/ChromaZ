$ErrorActionPreference = 'Stop'

$packageArgs = @{
  packageName    = 'chromaz'
  fileType       = 'exe'
  url64          = '[[URL64]]'
  silentArgs     = '/S'
  validExitCodes = @(0)
  checksum64     = '[[CHECKSUM]]'
  checksumType64 = 'sha256'
}

Install-ChocolateyPackage @packageArgs
