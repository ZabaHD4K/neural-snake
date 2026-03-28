$ffmpeg = "C:\Users\aleja\AppData\Local\Microsoft\WinGet\Packages\Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe\ffmpeg-8.1-full_build\bin\ffmpeg.exe"
$dir = "C:\Users\aleja\Desktop\proyectospersonales\neural-snake\videos"
Get-ChildItem "$dir\*.mp4" | ForEach-Object {
    $tmp = Join-Path $dir ("_tmp_" + $_.Name)
    & $ffmpeg -y -i $_.FullName -an -c:v copy $tmp 2>&1 | Out-Null
    if (Test-Path $tmp) {
        Move-Item -Force $tmp $_.FullName
        Write-Host ("OK: " + $_.Name)
    }
}
Write-Host "Done"
