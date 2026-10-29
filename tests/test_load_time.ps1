<#
@file test_load_time.ps1
@brief 验证物体、力场和导轨按时间加载并写入 EPSX。
#>
param([string]$Executable, [string]$WorkDir)
$ErrorActionPreference = 'Stop'
$scenePath = Join-Path $WorkDir 'load_time_test.epscene'
$epsxPath = Join-Path $WorkDir 'load_time_test.epsx'
$scene = '{"models":[{"category":0,"type":8,"id":"1","name":"late","load_time":0.05,"pos":[0,0,0],"initial_velocity":[10,0,0],"initial_angular_velocity":[0,0,0],"rotation":[1,0,0,0],"scale":[1,1,1],"mass":1,"restitution":0,"radius":1},{"category":1,"type":1,"id":"2","name":"gravity","load_time":0.1,"field_data":{"gravity":[0,0,-10]}},{"category":2,"type":4,"id":"3","name":"rail","load_time":0.15,"entity_ids":["1"],"constraint_data":{"start":[-100,0,0],"end":[100,0,0]}}]}'
Set-Content -LiteralPath $scenePath -Value $scene -Encoding utf8
& $Executable --solve $scenePath --duration 0.2 --sample 10 --step 1 -o $epsxPath | Out-Null
if ($LASTEXITCODE -ne 0) { throw '离线求解失败' }

<# @brief 读取指定 EPSX 帧，并在失败时中止测试。 #>
function Read-Frame([int]$Index) {
    $json = & $Executable --parse epsx $Index $epsxPath
    if ($LASTEXITCODE -ne 0) { throw "解析帧 $Index 失败" }
    return $json | ConvertFrom-Json
}
$before = Read-Frame 4
$bodyStart = Read-Frame 5
$fieldStart = Read-Frame 10
$railStart = Read-Frame 15
$end = Read-Frame 20
if ($before.bodies[0].stateFlags -ne 0 -or $before.bodies[0].position[0] -ne 0 -or $before.fields[0].enabled) { throw '加载前对象或力场已活动' }
if (($bodyStart.bodies[0].stateFlags -band 6) -ne 6 -or $bodyStart.fields[0].enabled) { throw '物体加载时间错误' }
if (-not $fieldStart.fields[0].enabled -or [Math]::Abs($fieldStart.bodies[0].position[0] - 0.5) -gt 1e-6) { throw '力场加载时间或物体轨迹错误' }
if (-not $railStart.constraints[0].enabled -or $railStart.bodies[0].position[2] -ge -0.01) { throw '导轨加载时间或重力轨迹错误' }
if ([Math]::Abs($end.bodies[0].position[2]) -gt 1e-6 -or $end.constraints[0].error -gt 1e-6) { throw '加载后的导轨约束未生效' }
