<#
.SYNOPSIS
    重建结果与基线清单的比对校验。
#>

function Invoke-StepVerify {
    <#
    .SYNOPSIS
        对比目标目录与 baseline\dependency_manifest.csv，输出缺失 / 多余 / 大小不一致清单。
    .NOTES
        比对的是“文件集合与文件大小”。重新编译得到的二进制在字节层面与原始文件不同（编译器补丁版本、
        时间戳等差异），因此不做 SHA256 等值判断，仅将其作为参考信息记录在报告中。
    #>
    Write-Head '步骤：与基线清单比对'

    $baselineFile = Join-Path $script:BaselineDir 'dependency_manifest.csv'
    if (-not (Test-Path -LiteralPath $baselineFile)) { throw "缺少基线清单: $baselineFile" }

    $root = $script:OutputDir
    if (-not (Test-Path -LiteralPath $root)) { throw "待校验目录不存在: $root" }

    Write-Info "基线清单: $baselineFile"
    Write-Info "待校验目录: $root"

    $baseline = Import-Csv -LiteralPath $baselineFile
    $bmap = @{}
    foreach ($row in $baseline) { $bmap[$row.Path] = $row }

    $current = Get-DirectoryManifest -Root $root -WithHash
    $cmap = @{}
    foreach ($row in $current) { $cmap[$row.Path] = $row }

    $missing   = New-Object System.Collections.Generic.List[string]
    $extra     = New-Object System.Collections.Generic.List[string]
    $sizeDiff  = New-Object System.Collections.Generic.List[object]   # 行尾不同、内容等价
    $contentDiff = New-Object System.Collections.Generic.List[object] # 内容确实不同
    $identical = 0

    foreach ($key in $bmap.Keys) {
        if (-not $cmap.ContainsKey($key)) { $missing.Add($key); continue }

        $b = $bmap[$key]
        $c = $cmap[$key]
        $bl = [int64]$b.Length
        $cl = [int64]$c.Length

        if ($bl -eq $cl -and $b.Sha256 -eq $c.Sha256) { $identical++; continue }

        $sameContent = ($b.Sha256Lf -and $c.Sha256Lf -and $b.Sha256Lf -eq $c.Sha256Lf)
        if ($sameContent) {
            $sizeDiff.Add([pscustomobject]@{ Path = $key; Baseline = $bl; Current = $cl })
        } else {
            $contentDiff.Add([pscustomobject]@{ Path = $key; Baseline = $bl; Current = $cl })
        }
    }
    foreach ($key in $cmap.Keys) {
        if (-not $bmap.ContainsKey($key)) { $extra.Add($key) }
    }

    # ------------------------------------------------------------ 报告 --
    $report = Join-Path $script:BuildRoot 'verification_report.md'
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.AppendLine('# dependency 重建校验报告')
    [void]$sb.AppendLine('')
    [void]$sb.AppendLine("- 生成时间：$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
    [void]$sb.AppendLine("- 基线清单：``$baselineFile``")
    [void]$sb.AppendLine("- 校验目录：``$root``")
    [void]$sb.AppendLine('')
    [void]$sb.AppendLine('| 项目 | 数量 |')
    [void]$sb.AppendLine('| --- | --- |')
    [void]$sb.AppendLine("| 基线文件数 | $($bmap.Count) |")
    [void]$sb.AppendLine("| 实际文件数 | $($cmap.Count) |")
    [void]$sb.AppendLine("| 内容完全一致 | $identical |")
    [void]$sb.AppendLine("| 仅行尾不同（CRLF/LF，内容等价） | $($sizeDiff.Count) |")
    [void]$sb.AppendLine("| 内容不一致 | $($contentDiff.Count) |")
    [void]$sb.AppendLine("| 缺失文件 | $($missing.Count) |")
    [void]$sb.AppendLine("| 多余文件 | $($extra.Count) |")
    [void]$sb.AppendLine('')
    if ($missing.Count -gt 0) {
        [void]$sb.AppendLine('## 缺失文件')
        [void]$sb.AppendLine('')
        foreach ($m in $missing) { [void]$sb.AppendLine("- ``$m``") }
        [void]$sb.AppendLine('')
    }
    if ($extra.Count -gt 0) {
        [void]$sb.AppendLine('## 多余文件')
        [void]$sb.AppendLine('')
        foreach ($e in $extra) { [void]$sb.AppendLine("- ``$e``") }
        [void]$sb.AppendLine('')
    }
    if ($sizeDiff.Count -gt 0) {
        [void]$sb.AppendLine('## 仅行尾不同的文件（CRLF <-> LF）')
        [void]$sb.AppendLine('')
        [void]$sb.AppendLine('')
        [void]$sb.AppendLine('| 文件 | 基线字节 | 实际字节 |')
        [void]$sb.AppendLine('| --- | --- | --- |')
        foreach ($d in $sizeDiff) { [void]$sb.AppendLine("| ``$($d.Path)`` | $($d.Baseline) | $($d.Current) |") }
        [void]$sb.AppendLine('')
    }
    if ($contentDiff.Count -gt 0) {
        [void]$sb.AppendLine('## 内容不一致的文件')
        [void]$sb.AppendLine('')
        [void]$sb.AppendLine('| 文件 | 基线字节 | 实际字节 |')
        [void]$sb.AppendLine('| --- | --- | --- |')
        foreach ($d in $contentDiff) { [void]$sb.AppendLine("| ``$($d.Path)`` | $($d.Baseline) | $($d.Current) |") }
        [void]$sb.AppendLine('')
    }
    Set-Content -LiteralPath $report -Value $sb.ToString() -Encoding UTF8

    # ------------------------------------------------------------ 输出 --
    Write-Host ''
    Write-Host ("  基线文件数   : {0}" -f $bmap.Count)
    Write-Host ("  实际文件数   : {0}" -f $cmap.Count)
    Write-Host ("  内容一致     : {0}" -f $identical) -ForegroundColor Green

    $color = if ($missing.Count -eq 0) { 'Green' } else { 'Red' }
    Write-Host ("  缺失文件     : {0}" -f $missing.Count) -ForegroundColor $color
    Write-Host ("  多余文件     : {0}" -f $extra.Count) -ForegroundColor $(if ($extra.Count -eq 0) { 'Green' } else { 'Yellow' })
    Write-Host ("  仅行尾不同   : {0}" -f $sizeDiff.Count) -ForegroundColor $(if ($sizeDiff.Count -eq 0) { 'Green' } else { 'Yellow' })
    Write-Host ("  内容不一致   : {0}" -f $contentDiff.Count) -ForegroundColor $(if ($contentDiff.Count -eq 0) { 'Green' } else { 'Red' })

    foreach ($m in ($missing | Select-Object -First 30)) { Write-Host "    [缺失] $m" -ForegroundColor Red }
    if ($missing.Count -gt 30) { Write-Host "    ... 其余 $($missing.Count - 30) 项见报告" -ForegroundColor Red }
    foreach ($d in ($contentDiff | Select-Object -First 20)) { Write-Host "    [内容不同] $($d.Path)" -ForegroundColor Red }
    foreach ($e in ($extra | Select-Object -First 20)) { Write-Host "    [多余] $e" -ForegroundColor Yellow }
    if ($extra.Count -gt 20) { Write-Host "    ... 其余 $($extra.Count - 20) 项见报告" -ForegroundColor Yellow }

    Write-Ok "校验报告: $report"
    if ($missing.Count -gt 0) { Write-Warn2 '存在缺失文件，请检查对应构建步骤的产物整理逻辑。' }
}
