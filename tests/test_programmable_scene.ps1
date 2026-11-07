<#
@file test_programmable_scene.ps1
@brief 验证 epscene 多段源码、动态增删、EPSX 动作记录与直接求解流程。
#>
param([string]$Executable,[string]$WorkDir)
$ErrorActionPreference='Stop'
$scenePath=Join-Path $WorkDir 'programmable_scene.epscene'
$resultPath=Join-Path $WorkDir 'programmable_scene.epsx'
$body=@{category=0;type=1;id='1';name='模板物体';mass=1;box_size=@(0.1,0.1,0.1);pos=@(0,0,0);rotation_enabled=$false;initial_velocity=@(0,0,0);scale=@(1,1,1)}
$source=@'
/** @brief 定时创建并配置替身，随后永久销毁。 */
void on_step(void) {
    if (simulation_time() >= 0.1 && !state_bool("created")) {
        Entity e = scene_spawn(scene_template("body"), vector3(2,0,0));
        entity_set_velocity(e,vector3(3,0,0));
        state_set_entity("new",e);
        state_set_bool("created",true);
    }
    if (simulation_time() >= 0.2 && !state_bool("removed_original")) {
        entity_destroy(scene_entity("body"));
        state_set_bool("removed_original",true);
    }
    if (simulation_time() >= 0.3 && !state_bool("removed_new")) {
        entity_destroy(state_entity("new"));
        state_set_bool("removed_new",true);
    }
}
'@
$second=@'
/** @brief 第二程序有独立状态，只在初始时刻设置速度。 */
void on_step(void) {
    if (!state_bool("created")) {
        entity_set_velocity(scene_entity("body"),vector3(1,0,0));
        state_set_bool("created",true);
    }
}
'@
$scene=@{models=@($body);scene_settings=@{programs=@(
    @{version=1;name='生命周期控制';enabled=$true;source=$source;bindings=@{body='1'};period=0.01},
    @{version=1;name='独立初始化';enabled=$true;source=$second;bindings=@{body='1'}}
)}}
$scene|ConvertTo-Json -Depth 15|Set-Content -LiteralPath $scenePath -Encoding utf8
& $Executable --solve $scenePath --duration 0.4 --step 1 --sample 10 -o $resultPath | Out-Null
if($LASTEXITCODE -ne 0){throw 'C 场景直接求解失败'}
<# @brief 用正式读取器解析指定帧。 #>
function Read-Frame([int]$Index){$json=& $Executable --parse epsx $Index $resultPath;if($LASTEXITCODE -ne 0){throw '动态结果读取失败'};return $json|ConvertFrom-Json}
$initial=Read-Frame 0;$spawned=Read-Frame 11;$removed=Read-Frame 21;$empty=Read-Frame 40
if($initial.bodies.Count -ne 1 -or $spawned.bodies.Count -ne 2 -or $removed.bodies.Count -ne 1 -or $empty.bodies.Count -ne 0){throw '动态实体生命周期数量错误'}
if($spawned.dynamicBodies.Count -ne 1 -or $spawned.dynamicBodies[0].templateId -ne '1'){throw '动态模板来源丢失'}
$newBody=$spawned.bodies|Where-Object {$_.position[0] -gt 1}
if([Math]::Abs($newBody.linearVelocity[0]-3) -gt 1e-9){throw '同批创建后配置速度失败'}
$events=@();foreach($i in 1..40){$events+=@( (Read-Frame $i).controlEvents )}
if(@($events|Where-Object kind -EQ 7).Count -ne 1 -or @($events|Where-Object kind -EQ 8).Count -ne 2){throw '动态生成与永久销毁事件丢失'}
if(@($events|Where-Object {$_.kind -eq 1 -and $_.target -eq '1'}).Count -ne 1){throw '多程序状态未隔离'}
Copy-Item -LiteralPath $resultPath -Destination (Join-Path $WorkDir 'programmable_dynamic.epsx') -Force
# 编译失败必须发生在输出文件创建之前。
$invalidPath=Join-Path $WorkDir 'programmable_invalid.epsx'
$scene.scene_settings.programs[0].source='void on_step(void) { invalid syntax; }'
$scene|ConvertTo-Json -Depth 15|Set-Content -LiteralPath $scenePath -Encoding utf8
$ErrorActionPreference='Continue'
& $Executable --solve $scenePath --duration 0.1 --step 1 --sample 10 -o $invalidPath *> (Join-Path $WorkDir 'programmable_invalid.log')
$invalidExit=$LASTEXITCODE
$ErrorActionPreference='Stop'
if($invalidExit -eq 0 -or (Test-Path $invalidPath)){throw '编译失败仍启动了结果写入'}
$body.rotation_enabled=$true
$body.box_size=@(1,1,1)
$scene.scene_settings.programs=@(@{version=1;name='转动与材质控制';bindings=@{body='1'};source=@'
/** @brief 持久力矩、姿态、质量与材质的真实刚体测试。 */
void on_step(void) {
    if (!state_bool("initialized")) {
        Entity body=scene_entity("body");
        entity_set_mass(body,2);
        entity_set_rotation(body,rotation(0,0,0,2));
        entity_set_material(body,material(0.7,0.4,0.2));
        entity_set_torque(body,vector3(0,0,2));
        state_set_bool("initialized",true);
    }
}
'@})
$scene|ConvertTo-Json -Depth 15|Set-Content -LiteralPath $scenePath -Encoding utf8
& $Executable --solve $scenePath --duration 0.1 --step 1 --sample 10 -o $resultPath | Out-Null
if($LASTEXITCODE -ne 0){throw '刚体转动控制求解失败'}
$rotated=Read-Frame 10
if($rotated.bodies[0].mass -ne 2 -or [Math]::Abs($rotated.bodies[0].angularVelocity[2]-0.6) -gt 0.01){throw '持久力矩或动态质量未正确更新转动'}
if([Math]::Abs($rotated.bodies[0].staticFriction-0.7) -gt 1e-12 -or [Math]::Abs($rotated.bodies[0].dynamicFriction-0.4) -gt 1e-12 -or [Math]::Abs($rotated.bodies[0].restitution-0.2) -gt 1e-12){throw '逐帧材质丢失'}
Write-Output 'C 场景动态求解、生命周期、转动、材质与多程序隔离验证通过'
