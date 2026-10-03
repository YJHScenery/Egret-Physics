<#
.SYNOPSIS
    记录现有 dependency 目录的基线清单，作为重建结果的校验基准。
#>

function Invoke-StepBaseline {
    <#
    .SYNOPSIS
        扫描依赖目录，生成 baseline\dependency_manifest.csv（相对路径、大小、SHA256）。
    .NOTES
        该步骤应在重建 dependency 之前执行，用于保存“当前环境”的完整清单。
    #>
    param([string]$Source)

    Write-Head '步骤：记录基线清单'

    if (-not $Source) {
        if ($BaselineSource) { $Source = $BaselineSource } else { $Source = $script:OutputDir }
    }
    if (-not (Test-Path -LiteralPath $Source)) { throw "待记录目录不存在: $Source" }

    $resolved = (Resolve-Path -LiteralPath $Source).Path.TrimEnd('\')
    Write-Info "扫描目录: $resolved"

    $rows = New-Object System.Collections.Generic.List[object]
    $files = Get-ChildItem -LiteralPath $resolved -Recurse -File -Force
    $i = 0
    foreach ($f in $files) {
        $i++
        $rel = $f.FullName.Substring($resolved.Length).TrimStart('\')
        $rows.Add([pscustomobject]@{
            Path     = $rel
            Length   = $f.Length
            Sha256   = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash
            Sha256Lf = (Get-NormalizedSha256 -Path $f.FullName)
        })
        if ($i % 2000 -eq 0) { Write-Host "  已处理 $i / $($files.Count)" -ForegroundColor DarkGray }
    }

    $outFile = Join-Path $script:BaselineDir 'dependency_manifest.csv'
    $rows | Export-Csv -LiteralPath $outFile -NoTypeInformation -Encoding UTF8

    $totalMb = [math]::Round((($files | Measure-Object -Property Length -Sum).Sum) / 1MB, 1)
    Write-Ok "基线清单已生成: $outFile（$($rows.Count) 个文件，$totalMb MB）"
}
