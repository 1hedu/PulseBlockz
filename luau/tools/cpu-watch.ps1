# Once a second: how fast the processor is running, how much of it Godot is using, and who else
# is busy. Laid beside a client's godot.log, it says whether a slowdown was the program getting
# more expensive, something else taking the processor, or the processor itself slowing down.
#
#   powershell -File luau\tools\cpu-watch.ps1 [-Minutes 60] [-Out cpu-watch.csv]
#
# % Processor Performance is the clock against its rated speed: 100 is 2.8 GHz on a part rated
# 2.8, above it is turbo, and a number that falls while the machine is busy is throttling.
# Process CPU is in per cents of ONE core, so 400 is four cores flat out.
param([int]$Minutes = 60, [string]$Out = "cpu-watch.csv")

$cores = (Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors
"time,clock_pct,mhz,busy_pct,godot_cpu_pct,godot_threads,other,other_cpu_pct" | Out-File -Encoding utf8 $Out
$until = (Get-Date).AddMinutes($Minutes)
while ((Get-Date) -lt $until) {
    $s = (Get-Counter '\Processor Information(_Total)\% Processor Performance',
                      '\Processor Information(_Total)\Processor Frequency',
                      '\Processor Information(_Total)\% Processor Utility',
                      '\Process(*)\% Processor Time' -SampleInterval 1 -MaxSamples 1 -ErrorAction SilentlyContinue).CounterSamples
    if (-not $s) { continue }
    $clock = [int]($s | Where-Object Path -like '*% processor performance').CookedValue
    $mhz   = [int]($s | Where-Object Path -like '*processor frequency').CookedValue
    $busy  = [int]($s | Where-Object Path -like '*% processor utility').CookedValue
    $procs = $s | Where-Object { $_.Path -like '*\process(*' -and $_.InstanceName -notin '_total', 'idle' }
    $godot = [int](($procs | Where-Object InstanceName -like 'godot*' | Measure-Object CookedValue -Sum).Sum)
    $threads = [int]((Get-Process -Name 'Godot*' -ErrorAction SilentlyContinue | ForEach-Object { $_.Threads.Count } | Measure-Object -Sum).Sum)
    $other = $procs | Where-Object InstanceName -notlike 'godot*' | Sort-Object CookedValue -Descending | Select-Object -First 1
    "{0},{1},{2},{3},{4},{5},{6},{7}" -f (Get-Date -Format 'HH:mm:ss'), $clock, $mhz, $busy, $godot, $threads, $other.InstanceName, [int]$other.CookedValue | Out-File -Encoding utf8 -Append $Out
}
