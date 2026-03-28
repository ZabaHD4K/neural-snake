Get-CimInstance Win32_Processor | Format-List Name, NumberOfCores, NumberOfLogicalProcessors
$ram = (Get-CimInstance Win32_PhysicalMemory | Measure-Object -Property Capacity -Sum).Sum / 1GB
Write-Host "RAM: $ram GB"
Get-CimInstance Win32_VideoController | Format-List Name, AdapterRAM
