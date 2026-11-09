<# @file test_vector_force_scene.ps1 @brief 验证受限 C 场从场景加载到物理求解。 #>
param([string]$Executable,[string]$WorkDir)
$ErrorActionPreference='Stop'
$scenePath=Join-Path $WorkDir 'vector_force_test.epscene'
$resultPath=Join-Path $WorkDir 'vector_force_test.epsx'
$body=@{category=0;type=1;id='1';name='物体';mass=1;box_size=@(0.1,0.1,0.1);pos=@(0,0,0);initial_velocity=@(0,0,0);scale=@(1,1,1)}
$source='bool field_contains(Vector3 p) { return p.x < 1; } Vector3 field_force(double t, Vector3 p) { return vector3(2, 0, 0); }'
$field=@{category=1;type=3;id='2';name='矢量场';field_data=@{source=$source}}
@{models=@($body,$field)} | ConvertTo-Json -Depth 15 | Set-Content -LiteralPath $scenePath -Encoding utf8
& $Executable --solve $scenePath --duration 0.1 --step 1 --sample 10 -o $resultPath | Out-Null
if($LASTEXITCODE -ne 0){throw '矢量场场景求解失败'}
$frame=(& $Executable --parse epsx 10 $resultPath | ConvertFrom-Json)
if($LASTEXITCODE -ne 0 -or [Math]::Abs($frame.bodies[0].linearVelocity[0]-0.2) -gt 0.01){throw '矢量场力未作用到物体'}
Write-Output '矢量场场景求解验证通过'
