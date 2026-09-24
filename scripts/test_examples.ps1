$ErrorActionPreference = 'Continue'

$rootDir = Split-Path -Parent $PSScriptRoot
$buildDir = if ([string]::IsNullOrWhiteSpace($env:BUILD_DIR)) {
    Join-Path $rootDir 'build'
} else {
    [Environment]::ExpandEnvironmentVariables($env:BUILD_DIR)
}
$outputRoot = if ([string]::IsNullOrWhiteSpace($env:OUTPUT_ROOT)) {
    'output'
} else {
    [Environment]::ExpandEnvironmentVariables($env:OUTPUT_ROOT)
}
$runId = if ([string]::IsNullOrWhiteSpace($env:RUN_ID)) {
    Get-Date -Format 'yyyyMMdd-HHmmss'
} else {
    $env:RUN_ID
}
$frameCount = if ([string]::IsNullOrWhiteSpace($env:FRAME_COUNT)) {
    '0'
} else {
    $env:FRAME_COUNT
}

$outDir = Join-Path $outputRoot $runId
$logDir = Join-Path $outDir 'logs'
$testFilesDir = Join-Path $outputRoot 'files'
$exampleFilesDir = Join-Path $rootDir 'examples/files'
$examplesDir = Join-Path $rootDir 'examples'
$plotMfccScript = Join-Path $PSScriptRoot 'plot_mfcc.py'

$passCount = 0
$failCount = 0
$skipCount = 0

New-Item -ItemType Directory -Force -Path $logDir | Out-Null

function Resolve-RequiredTool {
    param([string]$Name)

    $command = Get-Command $Name -CommandType Application -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        Write-Error "Required tool '$Name' was not found in PATH. Install GCC/G++ and Ninja, then retry."
        exit 1
    }
    return $command.Path
}

function Write-Note {
    param([string]$Message)
    Write-Host $Message
}

function Invoke-External {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,
        [Parameter(Mandatory = $false)]
        [string[]]$Arguments = @()
    )

    & $Command @Arguments | Out-Host
    $exitCode = $LASTEXITCODE
    return $exitCode
}

function Invoke-Case {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [string]$Artifact,
        [Parameter(Mandatory = $true)]
        [string]$Executable,
        [Parameter(Mandatory = $false)]
        [string[]]$Arguments = @()
    )

    $logFile = Join-Path $logDir "$Name.log"

    if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
        Write-Host "[SKIP] $Name missing executable: $Executable"
        $script:skipCount++
        return
    }

    Write-Host "[RUN ] $Name"
    & $Executable @Arguments > $logFile 2>&1
    $status = $LASTEXITCODE
    $summary = Get-Content -LiteralPath $logFile -Tail 1 -ErrorAction SilentlyContinue

    if ($status -eq 0) {
        Write-Host "      ok   $summary"
        $script:passCount++
    } else {
        Write-Host "      fail status=$status  log=$logFile"
        Get-Content -LiteralPath $logFile -Tail 8 -ErrorAction SilentlyContinue
        $script:failCount++
    }
}

function Invoke-StreamCase {
    param(
        [string]$Name,
        [string]$Codec,
        [string]$InputFile
    )

    $outputFile = Join-Path $outDir "$Name.pcm"
    Invoke-Case $Name $outputFile (Join-Path $exampleBinDir 'audio_codec_stream_demo.exe') @(
        $Codec, $InputFile, $outputFile, $frameCount
    )
}

function Invoke-ResampleCase {
    param(
        [string]$Name,
        [string]$Mode,
        [string]$Value
    )

    $outputFile = Join-Path $outDir "$Name.pcm"
    Invoke-Case $Name $outputFile (Join-Path $exampleBinDir 'resample_demo.exe') @(
        (Join-Path $testFilesDir 'jinitaimei.pcm'),
        $outputFile,
        $Mode,
        $Value,
        $frameCount
    )
}

function Invoke-MfccCase {
    param(
        [string]$Name,
        [string]$InputFile
    )

    $mfccDumpDir = Join-Path $outDir 'mfcc_dump'
    $outputFile = Join-Path $mfccDumpDir "$Name.bin"
    Invoke-Case $Name $outputFile (Join-Path $exampleBinDir 'mfcc_demo.exe') @(
        $InputFile, $outputFile
    )
    Invoke-External 'python' @($plotMfccScript, $outputFile) | Out-Null
}

function Invoke-BfccCase {
    param(
        [string]$Name,
        [string]$InputFile
    )

    $bfccDumpDir = Join-Path $outDir 'bfcc_dump'
    $outputFile = Join-Path $bfccDumpDir "$Name.bin"
    Invoke-Case $Name $outputFile (Join-Path $exampleBinDir 'bfcc_demo.exe') @(
        $InputFile, $outputFile
    )

    if (Test-Path -LiteralPath $outputFile -PathType Leaf) {
        $inputSamples = [int64]((Get-Item -LiteralPath $InputFile).Length / 2)
        $frameCountForInput = if ($inputSamples -ge 160) {
            [int64](($inputSamples - 160) / 160 + 1)
        } else {
            0
        }
        $actualSize = (Get-Item -LiteralPath $outputFile).Length
        $expectedSize = $frameCountForInput * 22 * 4
        if ($actualSize -ne $expectedSize) {
            Write-Host "      fail invalid output size: $actualSize/$expectedSize"
            $script:failCount++
            $script:passCount--
        }
    }
}

function Invoke-Ffmpeg {
    param([string[]]$Arguments)
    return Invoke-External 'ffmpeg' $Arguments
}

function Generate-TestFiles {
    param(
        [string]$InputWav,
        [string]$OutputDirectory
    )

    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

    if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
        Write-Note 'ffmpeg is required to generate test files'
        return $false
    }

    Write-Note "Generating ffmpeg test files into $OutputDirectory..."

    Write-Note 'Generating raw PCM input ...'
    $output = Join-Path $OutputDirectory 'jinitaimei.pcm'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-i', $InputWav, '-ar', '44100', '-ac', '2', '-f', 's16le', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    Write-Note 'Generating aac ...'
    $output = Join-Path $OutputDirectory 'jinitaimei.aac'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-i', $InputWav, '-c:a', 'aac', '-b:a', '128k', '-f', 'adts', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    Write-Note 'Generating amr ...'
    $output = Join-Path $OutputDirectory 'jinitaimei.amr'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-i', $InputWav, '-ar', '8000', '-ac', '1',
        '-c:a', 'libopencore_amrnb', '-b:a', '12.2k', '-f', 'amr', $output
    )) -ne 0) {
        Write-Note 'ffmpeg has no AMR-NB encoder, copying existing AMR sample'
        try {
            Copy-Item -Force -LiteralPath (Join-Path $exampleFilesDir 'jinitaimei.amr') -Destination $output
        } catch {
            Write-Note "failed: $output"
        }
    }

    Write-Note 'Generating flac ...'
    $output = Join-Path $OutputDirectory 'jinitaimei.flac'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-i', $InputWav, '-c:a', 'flac', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    Write-Note 'Generating m4a ...'
    $output = Join-Path $OutputDirectory 'jinitaimei.m4a'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-i', $InputWav, '-c:a', 'aac', '-b:a', '128k', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    Write-Note 'Generating mp3 ...'
    $output = Join-Path $OutputDirectory 'jinitaimei.mp3'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-i', $InputWav, '-c:a', 'libmp3lame', '-q:a', '4', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    Write-Note 'Generating ogg-opus ...'
    $output = Join-Path $OutputDirectory 'jinitaimei_opus.ogg'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-i', $InputWav, '-ar', '48000', '-c:a', 'libopus', '-b:a', '128k',
        '-f', 'ogg', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    Write-Note 'Generating ogg-vorbis ...'
    $output = Join-Path $OutputDirectory 'jinitaimei_vorbis.ogg'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-i', $InputWav, '-c:a', 'libvorbis', '-q:a', '5', '-f', 'ogg', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    Write-Note 'Generating g711a ...'
    $output = Join-Path $OutputDirectory 'jinitaimei_g711a.wav'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-i', $InputWav, '-c:a', 'pcm_alaw', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    Write-Note 'Generating g711u ...'
    $output = Join-Path $OutputDirectory 'jinitaimei_g711u.wav'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-i', $InputWav, '-c:a', 'pcm_mulaw', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    Write-Note 'Generating g722 ...'
    $output = Join-Path $OutputDirectory 'jinitaimei_g722.wav'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-i', $InputWav, '-ar', '16000', '-ac', '1', '-c:a', 'adpcm_g722', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    Write-Note 'Generating adpcm_ima ...'
    $output = Join-Path $OutputDirectory 'jinitaimei_adpcm_ima.wav'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-i', $InputWav, '-c:a', 'adpcm_ima_wav', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    Write-Note 'Generating caf-alac ...'
    $output = Join-Path $OutputDirectory 'jinitaimei_alac.caf'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-i', $InputWav, '-c:a', 'alac', '-f', 'caf', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    Write-Note 'Generating m4a-alac ...'
    $output = Join-Path $OutputDirectory 'jinitaimei_alac.m4a'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-i', $InputWav, '-c:a', 'alac', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    Write-Note 'Generating 24-bit caf-alac ...'
    $output = Join-Path $OutputDirectory 'jinitaimei_alac_24bit.caf'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-i', $InputWav, '-sample_fmt', 's32p', '-bits_per_raw_sample', '24',
        '-c:a', 'alac', '-f', 'caf', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    Write-Note 'Generating 3A test PCM...'
    $farOutput = Join-Path $testFilesDir 'jinitaimei_afe_3a_far.pcm'
    $nearOutput = Join-Path $testFilesDir 'jinitaimei_afe_3a_near.pcm'
    $filterComplex = (
        '[0:a]aresample=16000,pan=mono|c0=0.5*c0+0.5*c1,atrim=duration=5,asetpts=PTS-STARTPTS[voice];' +
        '[1:a]asplit=2[far][farecho];' +
        '[farecho]aecho=0.9:0.95:40|80|120:0.5|0.35|0.2[echoed];' +
        '[voice][echoed][2:a]amix=inputs=3:duration=first[near]'
    )
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-stream_loop', '-1', '-i', $InputWav,
        '-f', 'lavfi', '-i', 'sine=frequency=440:duration=5',
        '-f', 'lavfi', '-i', 'anoisesrc=color=pink:duration=5:amplitude=0.08',
        '-filter_complex', $filterComplex,
        '-map', '[far]', '-ar', '16000', '-ac', '1', '-f', 's16le', $farOutput,
        '-map', '[near]', '-ar', '16000', '-ac', '1', '-f', 's16le', $nearOutput
    )) -ne 0) {
        Write-Note "failed: $farOutput"
        Write-Note "failed: $nearOutput"
    }

    Write-Note 'Generating MFCC test PCM...'
    $output = Join-Path $OutputDirectory 'jinitaimei_mfcc_yes_1000ms_16k.pcm'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-i', (Join-Path $exampleFilesDir 'testdata/yes_1000ms.wav'),
        '-ar', '16000', '-ac', '1', '-f', 's16le', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    $output = Join-Path $OutputDirectory 'jinitaimei_mfcc_no_1000ms_16k.pcm'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-i', (Join-Path $exampleFilesDir 'testdata/no_1000ms.wav'),
        '-ar', '16000', '-ac', '1', '-f', 's16le', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    $output = Join-Path $OutputDirectory 'jinitaimei_mfcc_noise_1000ms_16k.pcm'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-i', (Join-Path $exampleFilesDir 'testdata/noise_1000ms.wav'),
        '-ar', '16000', '-ac', '1', '-f', 's16le', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    $output = Join-Path $OutputDirectory 'jinitaimei_mfcc_silence_1000ms_16k.pcm'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-i', (Join-Path $exampleFilesDir 'testdata/silence_1000ms.wav'),
        '-ar', '16000', '-ac', '1', '-f', 's16le', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    Write-Note 'Generating howling test PCM...'
    $output = Join-Path $OutputDirectory 'jinitaimei_howling.pcm'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-stream_loop', '-1', '-i', $InputWav,
        '-f', 'lavfi', '-i', 'sine=frequency=2600:sample_rate=44100:duration=8',
        '-filter_complex', '[0:a]atrim=duration=8,asetpts=PTS-STARTPTS[voice];[1:a]volume=0.8[howl];[voice][howl]amix=inputs=2:duration=first:normalize=0[mix]',
        '-map', '[mix]', '-ar', '44100', '-ac', '2', '-f', 's16le', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    Write-Note 'Generating filter low/mid/high test PCM...'
    $output = Join-Path $OutputDirectory 'ae_filter_low_mid_high_tones.pcm'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-f', 'lavfi', '-i', 'sine=frequency=500:sample_rate=44100:duration=8',
        '-f', 'lavfi', '-i', 'sine=frequency=6000:sample_rate=44100:duration=8',
        '-f', 'lavfi', '-i', 'sine=frequency=10000:sample_rate=44100:duration=8',
        '-filter_complex', '[0:a]volume=0.45[low];[1:a]volume=0.35[mid];[2:a]volume=0.25[high];[low][mid][high]amix=inputs=3:duration=first:normalize=0[mix]',
        '-map', '[mix]', '-ar', '44100', '-ac', '2', '-f', 's16le', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    Write-Note 'Generating mixer second input PCM (220 Hz sine)...'
    $duration = & ffprobe -v error '-show_entries' 'format=duration' '-of' 'default=noprint_wrappers=1:nokey=1' $InputWav 2>$null |
        Select-Object -Last 1
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($duration)) {
        $duration = '5'
    } else {
        $duration = $duration.ToString().Trim()
    }
    $output = Join-Path $OutputDirectory 'ae_mixer_sin220.pcm'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-f', 'lavfi', '-i', 'sine=frequency=220:sample_rate=44100',
        '-map', '0:a', '-ac', '2', '-ar', '44100', '-f', 's16le',
        '-t', $duration, $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    Write-Note 'Generating limiter & compressor test PCM...'
    $output = Join-Path $OutputDirectory 'jinitaimei_limiter_compressor.pcm'
    if ((Invoke-Ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-stream_loop', '-1', '-i', $InputWav,
        '-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=44100:duration=8',
        '-filter_complex', '[0:a]atrim=duration=8,volume=2.5[voice];[1:a]volume=1.6[tone];[voice][tone]amix=inputs=2:duration=first:normalize=0[mix]',
        '-map', '[mix]', '-ar', '44100', '-ac', '2', '-f', 's16le', $output
    )) -ne 0) {
        Write-Note "failed: $output"
    }

    return $true
}

$gccPath = Resolve-RequiredTool 'gcc'
$gxxPath = Resolve-RequiredTool 'g++'
$ninjaPath = Resolve-RequiredTool 'ninja'
$cmakePath = Resolve-RequiredTool 'cmake'
if ([IO.Path]::GetDirectoryName($gccPath) -ine [IO.Path]::GetDirectoryName($gxxPath)) {
    Write-Error "gcc and g++ must come from the same directory: '$gccPath' vs '$gxxPath'."
    exit 1
}

Write-Note 'CherryAVC example test'
Write-Note "  root       : $rootDir"
Write-Note "  build_dir  : $buildDir"
Write-Note "  out_dir    : $outDir"
Write-Note "  frames     : $frameCount (0 means all)"
Write-Note "  c_compiler : $gccPath"
Write-Note "  cxx_compiler: $gxxPath"
Write-Note "  ninja      : $ninjaPath"
Write-Note "  generator  : Ninja"
Write-Note ''

function Get-CMakeCacheValue {
    param(
        [string]$CacheFile,
        [string]$Key
    )

    if (-not (Test-Path -LiteralPath $CacheFile -PathType Leaf)) {
        return $null
    }

    $line = Select-String -LiteralPath $CacheFile -Pattern "^$([regex]::Escape($Key)):[^=]+=.*$" |
        Select-Object -First 1
    if ($null -eq $line) {
        return $null
    }
    return ($line.Line -replace "^[^=]+=", '')
}

Write-Note 'Configuring build directory...'
$cmakeStatus = Invoke-External $cmakePath @(
    '-S', $examplesDir,
    '-B', $buildDir,
    '-G', 'Ninja'
)
if ($cmakeStatus -ne 0) {
    exit $cmakeStatus
}

Write-Note 'Building examples...'
$cmakeStatus = Invoke-External $cmakePath @('--build', $buildDir)
if ($cmakeStatus -ne 0) {
    exit $cmakeStatus
}
Write-Note ''

$exampleBinDir = Join-Path $buildDir 'examples'
$exampleProbe = Join-Path $exampleBinDir 'audio_codec_stream_demo.exe'
if (-not (Test-Path -LiteralPath $exampleProbe -PathType Leaf)) {
    foreach ($configuration in @('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')) {
        $configurationBinDir = Join-Path $exampleBinDir $configuration
        if (Test-Path -LiteralPath (Join-Path $configurationBinDir 'audio_codec_stream_demo.exe') -PathType Leaf) {
            $exampleBinDir = $configurationBinDir
            break
        }
    }
}

if (-not (Generate-TestFiles (Join-Path $exampleFilesDir 'jinitaimei.wav') $testFilesDir)) {
    exit 1
}
Write-Note ''

Write-Note 'Dedicated demos'
New-Item -ItemType Directory -Force -Path (Join-Path $outDir 'avi_dump') | Out-Null
Invoke-Case 'avi_demo' (Join-Path $outDir 'avi_dump') (Join-Path $exampleBinDir 'avi_demo.exe') @(
    (Join-Path $exampleFilesDir 'jinitaimei_480x272.avi'),
    (Join-Path $outDir 'avi_dump'),
    $frameCount
)

New-Item -ItemType Directory -Force -Path (Join-Path $outDir 'mp4_dump') | Out-Null
Invoke-Case 'mp4_demo' (Join-Path $outDir 'mp4_dump') (Join-Path $exampleBinDir 'mp4_demo.exe') @(
    (Join-Path $exampleFilesDir 'jinitaimei_480x272.mp4'),
    (Join-Path $outDir 'mp4_dump'),
    $frameCount
)

Write-Note ''
Write-Note 'MFCC demo'
New-Item -ItemType Directory -Force -Path (Join-Path $outDir 'mfcc_dump') | Out-Null
Invoke-MfccCase 'mfcc_demo_yes_1000ms_16k' (Join-Path $testFilesDir 'jinitaimei_mfcc_yes_1000ms_16k.pcm')
Invoke-MfccCase 'mfcc_demo_no_1000ms_16k' (Join-Path $testFilesDir 'jinitaimei_mfcc_no_1000ms_16k.pcm')
Invoke-MfccCase 'mfcc_demo_noise_1000ms_16k' (Join-Path $testFilesDir 'jinitaimei_mfcc_noise_1000ms_16k.pcm')
Invoke-MfccCase 'mfcc_demo_silence_1000ms_16k' (Join-Path $testFilesDir 'jinitaimei_mfcc_silence_1000ms_16k.pcm')

Write-Note ''
Write-Note 'BFCC demo'
New-Item -ItemType Directory -Force -Path (Join-Path $outDir 'bfcc_dump') | Out-Null
Invoke-BfccCase 'bfcc_demo_yes_1000ms_16k' (Join-Path $testFilesDir 'jinitaimei_mfcc_yes_1000ms_16k.pcm')
Invoke-BfccCase 'bfcc_demo_no_1000ms_16k' (Join-Path $testFilesDir 'jinitaimei_mfcc_no_1000ms_16k.pcm')
Invoke-BfccCase 'bfcc_demo_noise_1000ms_16k' (Join-Path $testFilesDir 'jinitaimei_mfcc_noise_1000ms_16k.pcm')
Invoke-BfccCase 'bfcc_demo_silence_1000ms_16k' (Join-Path $testFilesDir 'jinitaimei_mfcc_silence_1000ms_16k.pcm')

Write-Note ''
Write-Note 'AFE 3A demo'
Invoke-Case 'afe_3a_demo' (Join-Path $outDir 'afe_3a_demo.pcm') (Join-Path $exampleBinDir 'afe_3a_demo.exe') @(
    (Join-Path $testFilesDir 'jinitaimei_afe_3a_near.pcm'),
    (Join-Path $testFilesDir 'jinitaimei_afe_3a_far.pcm'),
    (Join-Path $outDir 'afe_3a_demo.pcm'),
    '16000',
    $frameCount
)

Write-Note ''
Write-Note 'AFE Howling demo'
Invoke-Case 'afe_howling' (Join-Path $outDir 'afe_howling.pcm') (Join-Path $exampleBinDir 'afe_howling_demo.exe') @(
    (Join-Path $testFilesDir 'jinitaimei_howling.pcm'),
    (Join-Path $outDir 'afe_howling.pcm'),
    '8', '6', '6', '12', '4', $frameCount
)

Write-Note ''
Write-Note 'AE Sonic demo'
Invoke-Case 'sonic_speed' (Join-Path $outDir 'sonic_speed.pcm') (Join-Path $exampleBinDir 'ae_sonic_demo.exe') @(
    (Join-Path $testFilesDir 'jinitaimei.pcm'),
    (Join-Path $outDir 'sonic_speed.pcm'),
    '1.35', '1.00', $frameCount
)
Invoke-Case 'sonic_pitch' (Join-Path $outDir 'sonic_pitch.pcm') (Join-Path $exampleBinDir 'ae_sonic_demo.exe') @(
    (Join-Path $testFilesDir 'jinitaimei.pcm'),
    (Join-Path $outDir 'sonic_pitch.pcm'),
    '1.00', '1.20', $frameCount
)

Write-Note ''
Write-Note 'AE Volume demo'
Invoke-Case 'ae_vol_down' (Join-Path $outDir 'ae_vol_down.pcm') (Join-Path $exampleBinDir 'ae_vol_demo.exe') @(
    (Join-Path $testFilesDir 'jinitaimei.pcm'),
    (Join-Path $outDir 'ae_vol_down.pcm'),
    '128', '-60', '18', $frameCount
)
Invoke-Case 'ae_vol_up' (Join-Path $outDir 'ae_vol_up.pcm') (Join-Path $exampleBinDir 'ae_vol_demo.exe') @(
    (Join-Path $testFilesDir 'jinitaimei.pcm'),
    (Join-Path $outDir 'ae_vol_up.pcm'),
    '240', '-60', '18', $frameCount
)

Write-Note ''
Write-Note 'AE Mixer demo'
Invoke-Case 'ae_mixer' (Join-Path $outDir 'ae_mixer.pcm') (Join-Path $exampleBinDir 'ae_mixer_demo.exe') @(
    (Join-Path $testFilesDir 'jinitaimei.pcm'),
    (Join-Path $testFilesDir 'ae_mixer_sin220.pcm'),
    (Join-Path $outDir 'ae_mixer.pcm'),
    '0.75', '0.50', $frameCount
)

Write-Note ''
Write-Note 'AE Reverb demo'
Invoke-Case 'ae_reverb' (Join-Path $outDir 'ae_reverb.pcm') (Join-Path $exampleBinDir 'ae_reverb_demo.exe') @(
    (Join-Path $testFilesDir 'jinitaimei.pcm'),
    (Join-Path $outDir 'ae_reverb.pcm'),
    $frameCount
)

Write-Note ''
Write-Note 'AE Compressor demo'
Invoke-Case 'ae_compressor' (Join-Path $outDir 'ae_compressor.pcm') (Join-Path $exampleBinDir 'ae_compressor_demo.exe') @(
    (Join-Path $testFilesDir 'jinitaimei_limiter_compressor.pcm'),
    (Join-Path $outDir 'ae_compressor.pcm'),
    $frameCount
)

Write-Note ''
Write-Note 'AE Limiter demo'
Invoke-Case 'ae_limiter' (Join-Path $outDir 'ae_limiter.pcm') (Join-Path $exampleBinDir 'ae_limiter_demo.exe') @(
    (Join-Path $testFilesDir 'jinitaimei_limiter_compressor.pcm'),
    (Join-Path $outDir 'ae_limiter.pcm'),
    $frameCount
)

Write-Note ''
Write-Note 'AE EQ demo'
Invoke-Case 'ae_eq' (Join-Path $outDir 'ae_eq.pcm') (Join-Path $exampleBinDir 'ae_eq_demo.exe') @(
    (Join-Path $testFilesDir 'jinitaimei.pcm'),
    (Join-Path $outDir 'ae_eq.pcm'),
    $frameCount
)

Write-Note ''
Write-Note 'AE Filter demo'
$filterTypes = @(
    'low_pass', 'high_pass', 'band_pass', 'band_stop',
    'all_pass', 'peaking', 'low_shelf', 'high_shelf'
)
for ($filterIndex = 0; $filterIndex -lt $filterTypes.Count; $filterIndex++) {
    $filterType = $filterTypes[$filterIndex]
    $filterOutput = Join-Path $outDir "ae_filter_$filterType.pcm"
    Invoke-Case "ae_filter_$filterType" $filterOutput (Join-Path $exampleBinDir 'ae_filter_demo.exe') @(
        (Join-Path $testFilesDir 'ae_filter_low_mid_high_tones.pcm'),
        $filterOutput,
        $frameCount,
        $filterIndex.ToString()
    )
}

Write-Note ''
Write-Note 'audio_codec_stream_demo'
Invoke-StreamCase 'stream_aac' 'aac' (Join-Path $testFilesDir 'jinitaimei.aac')
Invoke-StreamCase 'stream_amr' 'amr' (Join-Path $testFilesDir 'jinitaimei.amr')
Invoke-StreamCase 'stream_amr_wb' 'amr' (Join-Path $exampleFilesDir 'amr_wb_min.awb')
Invoke-StreamCase 'stream_flac' 'flac' (Join-Path $testFilesDir 'jinitaimei.flac')
Invoke-StreamCase 'stream_mp3' 'mp3' (Join-Path $testFilesDir 'jinitaimei.mp3')
Invoke-StreamCase 'stream_alac' 'alac' (Join-Path $testFilesDir 'jinitaimei_alac.caf')
Invoke-StreamCase 'stream_alac_24bit' 'alac' (Join-Path $testFilesDir 'jinitaimei_alac_24bit.caf')
Invoke-StreamCase 'stream_m4a' 'm4a' (Join-Path $testFilesDir 'jinitaimei.m4a')
Invoke-StreamCase 'stream_ogg_opus' 'ogg' (Join-Path $testFilesDir 'jinitaimei_opus.ogg')
Invoke-StreamCase 'stream_ogg_vorbis' 'ogg' (Join-Path $testFilesDir 'jinitaimei_vorbis.ogg')
Invoke-StreamCase 'stream_wav_pcm' 'wav' (Join-Path $exampleFilesDir 'jinitaimei.wav')
Invoke-StreamCase 'stream_wav_adpcm_ima' 'wav' (Join-Path $testFilesDir 'jinitaimei_adpcm_ima.wav')
Invoke-StreamCase 'stream_wav_g711a' 'wav' (Join-Path $testFilesDir 'jinitaimei_g711a.wav')
Invoke-StreamCase 'stream_wav_g711u' 'wav' (Join-Path $testFilesDir 'jinitaimei_g711u.wav')
Invoke-StreamCase 'stream_wav_g722' 'wav' (Join-Path $testFilesDir 'jinitaimei_g722.wav')

Invoke-Case 'wav_encode_demo' (Join-Path $outDir 'wav_encode_demo.wav') (Join-Path $exampleBinDir 'wav_encode_demo.exe') @(
    (Join-Path $outDir 'stream_wav_pcm.pcm'),
    (Join-Path $outDir 'wav_encode_demo.wav'),
    '44100', '16', '2'
)

Invoke-ResampleCase 'resample_rate_8k' 'rate' '8000'
Invoke-ResampleCase 'resample_rate_16k' 'rate' '16000'
Invoke-ResampleCase 'resample_rate_32k' 'rate' '32000'
Invoke-ResampleCase 'resample_rate_48k' 'rate' '48000'
Invoke-ResampleCase 'resample_bits_8' 'bit' '8'
Invoke-ResampleCase 'resample_bits_24' 'bit' '24'
Invoke-ResampleCase 'resample_bits_32' 'bit' '32'
Invoke-ResampleCase 'resample_ch_1' 'channel' '1'
Invoke-ResampleCase 'resample_ch_3' 'channel' '3'
Invoke-ResampleCase 'resample_ch_4' 'channel' '4'

Write-Note ''
Write-Note 'Summary'
Write-Note "  passed : $passCount"
Write-Note "  failed : $failCount"
Write-Note "  skipped: $skipCount"
Write-Note "  logs   : $logDir"

if ($failCount -ne 0) {
    exit 1
}
exit 0
