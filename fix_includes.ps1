# fix_includes.ps1 — 把 "include/cef_X" 替换为 "cef_X"
Get-ChildItem 'd:\workspace\webview_cef\common','d:\workspace\webview_cef\windows' -Include '*.h','*.cc','*.cpp' -Recurse | ForEach-Object {
    $content = Get-Content $_.FullName -Raw
    $new = $content -replace [regex]::Escape('"include/cef_'), '"cef_'
    $new = $new -replace [regex]::Escape('<include/cef_'), '<cef_'
    if ($new -ne $content) {
        Set-Content -NoNewline $_.FullName $new
        Write-Host "Fixed: $($_.FullName)"
    }
}
Write-Host "All done."
