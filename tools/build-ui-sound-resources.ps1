# Build the optional licensed UI sounds into the renderer, without placing
# standalone sound files in the public source or distribution.
# Run from a Visual Studio developer command prompt with Windows SDK rc.exe.
# Example: powershell -File tools/build-ui-sound-resources.ps1 -ExternalAudioDir X:\private\ui-keyboard-ai-v1
# Then run tools/build-performance.cmd, which links the resulting .res if present.
#
# AUDIO NOTICE (not covered by the source-code MIT license): these three effects
# were generated with fal / ElevenLabs Sound Effects V2 and are licensed only as
# part of the combined mod/application. Do not distribute their WAV files, this
# intermediate resource library, or extracted effects as standalone audio or as
# an audio library. Only the compiled renderer containing the effects is intended
# for the release. Retain the corresponding audio notice in the release notices.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$ExternalAudioDir,
    [string]$ResourceCompiler
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# SHA-256 metadata copied from the approved ui-keyboard-ai-v1 manifest. There are
# no encoded samples or generated sound data in this source file.
$cues = @(
    @{ Id = 301; File = 'hover.wav'; Sha256 = '59fa995b8bc78ab11f2eac38349fa1d12ef2311ee3c56047e394b3feeb09ebca' },
    @{ Id = 302; File = 'confirm.wav'; Sha256 = '72f5c8644ea19bb53ee85520ebc6c2836a0f17803a32ea3cb6613fece132edaf' },
    @{ Id = 303; File = 'back.wav'; Sha256 = 'bd97c3fa2f31a44a8fef366073eeacea22ad9c1b8c1ace63cef2b9ec252bd76c' }
)

$audioDirectory = Get-Item -LiteralPath $ExternalAudioDir -Force
if (-not $audioDirectory.PSIsContainer -or ($audioDirectory.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
    throw 'ExternalAudioDir must be an ordinary directory containing the approved sound effects.'
}
foreach ($cue in $cues) {
    $file = Get-Item -LiteralPath (Join-Path $audioDirectory.FullName $cue.File) -Force
    if ($file.PSIsContainer -or ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $file.Length -lt 44 -or $file.Length -gt 1MB) {
        throw ('Invalid sound-effect input: ' + $cue.File)
    }
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    if ($hash -ne $cue.Sha256) { throw ('Approved sound-effect SHA-256 mismatch: ' + $cue.File) }
    $cue.Path = $file.FullName
}

if ([string]::IsNullOrWhiteSpace($ResourceCompiler)) {
    $compilerCommand = Get-Command rc.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -eq $compilerCommand) {
        throw 'Windows SDK rc.exe was not found. Run this script in a Visual Studio developer command prompt, or pass -ResourceCompiler with its full path.'
    }
    $ResourceCompiler = $compilerCommand.Source
}
$compiler = Get-Item -LiteralPath $ResourceCompiler -Force
if ($compiler.PSIsContainer -or $compiler.Extension -ine '.exe') { throw 'ResourceCompiler must identify rc.exe.' }

$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$outputDirectory = [IO.Path]::GetFullPath((Join-Path $repository 'build\performance'))
if (-not $outputDirectory.StartsWith($repository + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Resource output must remain inside the repository build directory.'
}
[IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
if ((Get-Item -LiteralPath $outputDirectory -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
    throw 'The resource build directory cannot be a reparse point.'
}

$resourceSource = Join-Path $outputDirectory 'ui-sounds.rc'
$resourceOutput = Join-Path $outputDirectory 'ui-sounds.res'
$temporaryOutput = Join-Path $outputDirectory ('ui-sounds-' + [Guid]::NewGuid().ToString('N') + '.res')
$lines = @('// Generated from external licensed inputs; do not distribute as a standalone sound library.')
foreach ($cue in $cues) {
    # RC string literals escape backslashes. Windows filenames cannot contain a
    # quote, but reject line breaks as well before writing any resource source.
    if ($cue.Path.IndexOfAny([char[]]"`r`n`"") -ge 0) { throw 'A sound-effect path cannot contain quotes or line breaks.' }
    $escaped = $cue.Path.Replace('\', '\\')
    $lines += ('{0} RCDATA "{1}"' -f $cue.Id, $escaped)
}
# UTF-16 with a BOM preserves external Windows paths in rc.exe.
[IO.File]::WriteAllLines($resourceSource, $lines, [Text.Encoding]::Unicode)
try {
    & $compiler.FullName /nologo /fo $temporaryOutput $resourceSource
    if ($LASTEXITCODE -ne 0 -or -not [IO.File]::Exists($temporaryOutput)) {
        throw ('Windows resource compilation failed with exit code ' + $LASTEXITCODE + '.')
    }
    if ([IO.File]::Exists($resourceOutput)) {
        [IO.File]::Replace($temporaryOutput, $resourceOutput, $null)
    } else {
        [IO.File]::Move($temporaryOutput, $resourceOutput)
    }
} finally {
    if ([IO.File]::Exists($temporaryOutput)) { [IO.File]::Delete($temporaryOutput) }
}

Write-Output ('Prepared optional UI sound resources: ' + $resourceOutput)
Write-Output 'Next: tools\build-performance.cmd. Distribute only the combined renderer DLL and retain the audio license notice; do not publish standalone audio or resource libraries.'
