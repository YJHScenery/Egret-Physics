<#
@file test_scene_settings.ps1
@brief 验证表单 JSON 规则离线执行、尺度不改变物理结果及 EPSX 保存设置。
#>
param([string]$Executable,[string]$WorkDir)
$ErrorActionPreference='Stop'
$scenePath=Join-Path $WorkDir 'scene_settings_test.epscene'
$outputPath=Join-Path $WorkDir 'scene_settings_test.epsx'
$body=@{category=0;type=1;id='1';name='控制物体';mass=1;box_size=@(0.1,0.1,0.1);pos=@(0,0,0);rotation_enabled=$false;restitution=0;initial_velocity=@(0,0,0);scale=@(1,1,1)}
$rules=@(
    @{name='开始拉力';trigger='Once';conditions=@(@{kind='Time';comparison='GreaterEqual';lower=0.1});actions=@(@{kind='SetForce';target='1';value=@(2,0,0)})},
    @{name='取消拉力';trigger='Once';conditions=@(@{kind='Time';comparison='GreaterEqual';lower=0.3});actions=@(@{kind='SetForce';target='1';value=@(0,0,0)})},
    @{name='越界停用';trigger='Once';conditions=@(@{kind='Position';first='1';component='X';comparison='GreaterEqual';lower=0.08});actions=@(@{kind='SetActive';target='1';enabled=$false})}
)
<# @brief 调用官方解析命令读取结果帧。 #>
function Read-Frame([int]$Index) {
    $result=& $Executable --parse epsx $Index $outputPath
    if($LASTEXITCODE -ne 0){throw '无法读取结果帧'}
    return $result|ConvertFrom-Json
}
$positions=@()
foreach($reference in @(0.01,1000.0)){
    $scene=@{models=@($body);scene_settings=@{display=@{reference_length=$reference;markers=$true};rules=$rules}}
    $scene|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $scenePath -Encoding utf8
    & $Executable --solve $scenePath --duration 0.5 --step 1 --sample 10 -o $outputPath|Out-Null
    if($LASTEXITCODE -ne 0){throw '规则求解失败'}
    $before=Read-Frame 9;$moving=Read-Frame 29;$after=Read-Frame 31;$end=Read-Frame 50
    if($before.bodies[0].position[0] -ne 0){throw '规则在设定时间前执行'}
    if([Math]::Abs($moving.bodies[0].linearVelocity[0]-0.38) -gt 0.003){throw '定时控制力未生效'}
    if([Math]::Abs($after.bodies[0].linearVelocity[0]-0.4) -gt 0.003){throw '取消控制力失败'}
    if(($end.bodies[0].stateFlags -band 6) -ne 0 -or [Math]::Abs($end.bodies[0].position[0]-0.08) -gt 0.002){throw '阈值停用失败'}
    $positions+=,$end.bodies[0].position
    $bytes=[IO.File]::ReadAllBytes($outputPath)
    $offset=[BitConverter]::ToUInt64($bytes,40)
    $length=[BitConverter]::ToUInt64($bytes,[int]$offset+8)
    $saved=[Text.Encoding]::UTF8.GetString($bytes,[int]$offset+24,[int]$length)|ConvertFrom-Json
    if($saved.scene_settings.display.reference_length -ne $reference -or $saved.scene_settings.rules.Count -ne 3){throw 'EPSX 丢失显示配置或规则'}
}
if([Math]::Abs($positions[0][0]-$positions[1][0]) -gt 1e-12){throw '显示参考改变了物理轨迹'}
$scene.scene_settings.rules[0].actions[0].target='999'
$scene|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $scenePath -Encoding utf8
$previousPreference=$ErrorActionPreference
$ErrorActionPreference='Continue'
& $Executable --solve $scenePath --duration 0.5 --step 1 --sample 10 -o $outputPath *> (Join-Path $WorkDir 'scene_settings_invalid.log')
$invalidExitCode=$LASTEXITCODE
$ErrorActionPreference=$previousPreference
if($invalidExitCode -eq 0){throw '失效物体引用未在求解前拒绝'}
Write-Output '场景设置与控制规则端到端验证通过'
