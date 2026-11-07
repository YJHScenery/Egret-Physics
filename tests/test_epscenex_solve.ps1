<#
@file test_epscenex_solve.ps1
@brief 验证独立 ZIP 工具生成的 V2 场景可以直接求解，结果与 V1 一致。
#>
param([string]$Executable, [string]$WorkDir)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$legacyPath = Join-Path $WorkDir 'archive_solve.epscene'
$archivePath = Join-Path $WorkDir 'archive_solve.epscenex'
$source = @'
/** @brief 设置物体速度，检验归档源码确实参与求解。 */
void on_step(void) {
    entity_set_velocity(scene_entity("body"), vector3(3, 0, 0));
}
'@
$body = @{category=0;type=1;id='1';name='归档测试';mass=1;box_size=@(1,1,1);pos=@(0,0,0);scale=@(1,1,1);rotation_enabled=$false}
$settings = @{programs=@(@{version=1;name='独立源码';source=$source;bindings=@{body='1'}})}
@{version='1.0';models=@($body);scene_settings=$settings} | ConvertTo-Json -Depth 15 | Set-Content -LiteralPath $legacyPath -Encoding utf8
$settings.programs[0].source = 'control/program_1.c'
$metadata = @{magic='EPSCENE';version='2.0';entity_order=@(0);scene=@{version='1.0';scene_settings=$settings}}
$stream = [System.IO.File]::Open($archivePath, [System.IO.FileMode]::Create)
$zip = [System.IO.Compression.ZipArchive]::new($stream, [System.IO.Compression.ZipArchiveMode]::Create)
try {
    $entries = @{
        'metadata.json'=($metadata | ConvertTo-Json -Depth 15)
        'entities/general.json'=('[{0}]' -f ($body | ConvertTo-Json -Depth 10))
        'entities/fields.json'='[]'
        'entities/constraints.json'='[]'
        'control/program_1.c'=$source
    }
    foreach ($name in $entries.Keys) {
        $entry = $zip.CreateEntry($name)
        $writer = [System.IO.StreamWriter]::new($entry.Open(), [System.Text.UTF8Encoding]::new($false))
        try { $writer.Write($entries[$name]) } finally { $writer.Dispose() }
    }
    $null = $zip.CreateEntry('config/')
} finally { $zip.Dispose(); $stream.Dispose() }
$frames = @()
foreach ($path in @($legacyPath, $archivePath)) {
    $output = "$path.epsx"
    & $Executable --solve $path --duration 0.1 --step 1 --sample 10 -o $output | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "场景求解失败：$path" }
    $frame = & $Executable --parse epsx 10 $output
    if ($LASTEXITCODE -ne 0) { throw "结果读取失败：$path" }
    $frames += ($frame | ConvertFrom-Json)
}
if ($frames[1].bodies.Count -ne 1 -or [Math]::Abs($frames[1].bodies[0].linearVelocity[0] - 3) -gt 1e-9) {
    throw 'V2 归档源码未正确参与求解'
}
if (($frames[0].bodies | ConvertTo-Json -Depth 15 -Compress) -ne ($frames[1].bodies | ConvertTo-Json -Depth 15 -Compress)) {
    throw 'V1 与 V2 求解结果不一致'
}
Write-Output 'V1 与 V2 直接求解结果一致，独立 C 源码加载通过'
