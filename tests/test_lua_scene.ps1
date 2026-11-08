<#
@file test_lua_scene.ps1
@brief 验证 Lua 场景直接求解、EPSX 数据、动作事件及受控失败。
#>
param([Parameter(Mandatory=$true)][string]$Executable,[Parameter(Mandatory=$true)][string]$WorkDir)
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
$repo=Split-Path $PSScriptRoot -Parent
$resultPath=Join-Path $WorkDir 'lua_scene.epsx'
& $Executable --solve (Join-Path $repo 'demo/lua_body_control.epscene') --duration 5 --step 1 --sample 10 -o $resultPath | Out-Null
if($LASTEXITCODE -ne 0){throw 'Lua 场景直接求解失败'}
$frame=& $Executable --parse epsx 500 $resultPath | ConvertFrom-Json
if($LASTEXITCODE -ne 0){throw 'Lua 结果读取失败'}
if($frame.bodies.Count -ne 1){throw 'Lua 场景实体数量错误'}
$body=$frame.bodies[0]
if([Math]::Abs($body.linearVelocity[0]-4) -gt 0.01 -or [Math]::Abs($body.position[0]-12) -gt 0.02){throw 'Lua 定时施力结果错误'}
$scene=Get-Content (Join-Path $repo 'demo/lua_body_control.epscene') -Raw -Encoding utf8 | ConvertFrom-Json
$scene.scene_settings.programs[0].source='-- 死循环预算验证。'+"`n"+'function on_step() while true do end end'
$scene.scene_settings.programs[0].fuel=1000
$invalidScene=Join-Path $WorkDir 'lua_invalid.epscene'
$scene|ConvertTo-Json -Depth 25|Set-Content -LiteralPath $invalidScene -Encoding utf8
$invalidLog=Join-Path $WorkDir 'lua_invalid.log'
$ErrorActionPreference='Continue'
& $Executable --solve $invalidScene --duration 0.1 --step 1 --sample 10 -o (Join-Path $WorkDir 'lua_invalid.epsx') *> $invalidLog
$failureExit=$LASTEXITCODE
$ErrorActionPreference='Stop'
if($failureExit -eq 0){throw 'Lua 死循环没有终止求解'}
if(!(Select-String -LiteralPath $invalidLog -Pattern 'Lua|预算' -Quiet)){throw 'Lua 错误诊断缺失'}
Write-Output 'Lua 场景直接求解与失败处理通过'
