<# @file test_anchor_playback.ps1 @brief 验证自动锚点单摆求解后可由独立回放实例恢复。 #>
param([string]$Executable,[string]$Scene,[string]$WorkDir)
$ErrorActionPreference='Stop'
$result=Join-Path $WorkDir 'anchor_playback_test.epsx'
$video=Join-Path $WorkDir 'anchor_playback_test.mp4'
& $Executable --solve $Scene --duration 0.1 --step 1 --sample 10 -o $result | Out-Null
if($LASTEXITCODE -ne 0){throw '锚点场景求解失败'}
$ErrorActionPreference='Continue'
$output=& $Executable --export-video --input $result --output $video --width 320 --height 240 --fps 10 2>&1
$exportExit=$LASTEXITCODE
$ErrorActionPreference='Stop'
if($exportExit -ne 0 -or ($output -match '动态实体模板不存在')){throw ($output -join "`n")}
if(!(Test-Path $video) -or (Get-Item $video).Length -eq 0){throw '回放未生成视频'}