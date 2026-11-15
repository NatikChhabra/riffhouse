# Before/after listening clips for a build, rendered off-line from a tester's own clean guitar
# recordings. Runs unattended at low priority: it waits for the build's CI run, downloads the
# installer and the test tool, checks the installer's hash, renders every recording through each
# tone twice (the real amp captures, and the circuit models they replace), names the two at
# random A and B, and writes the key where the listener will not see it. Nothing is played aloud
# and no sound device is opened.
#
#   powershell -ExecutionPolicy Bypass -File render-ab.ps1 -Branch <branch> -DiFolder <folder of .wav> -OutRoot <folder>
param(
    [Parameter(Mandatory = $true)][string]$Branch,
    [Parameter(Mandatory = $true)][string[]]$DiFolder,
    [Parameter(Mandatory = $true)][string]$OutRoot,
    [string]$Repo = 'NatikChhabra/riffhouse',
    [string[]]$Tones = @('Heavy Starter', 'Djent Precision', 'Death Metal Buzz', 'Alt Rock Wall', 'Bangalore Thrash', 'Big Rock Rhythm'),
    [int]$MaxRecordings = 4,
    [int]$WaitMinutes = 180
)
$ErrorActionPreference = 'Stop'
try { (Get-Process -Id $PID).PriorityClass = 'BelowNormal' } catch {}

New-Item -ItemType Directory -Force -Path $OutRoot | Out-Null
$summary = Join-Path $OutRoot 'SUMMARY.txt'
function Say([string]$line) { $stamp = (Get-Date).ToString('HH:mm:ss'); "$stamp  $line" | Tee-Object -FilePath $summary -Append }
Say "Started for branch $Branch"

# 1. the newest CI run of the branch, waited for until it finishes
$deadline = (Get-Date).AddMinutes($WaitMinutes)
while ($true) {
    $run = gh run list -R $Repo --branch $Branch --workflow build.yml --limit 1 --json databaseId,status,conclusion,headSha | ConvertFrom-Json | Select-Object -First 1
    if ($run -and $run.status -eq 'completed') { break }
    if ((Get-Date) -gt $deadline) { Say "STOPPED: the CI run did not finish in $WaitMinutes minutes"; exit 1 }
    Start-Sleep -Seconds 120
}
$short = $run.headSha.Substring(0, 7)
Say "CI run $($run.databaseId) on $short finished: $($run.conclusion)"
if ($run.conclusion -ne 'success') { Say 'STOPPED: the build or its tests failed, nothing rendered'; exit 1 }

# 2. the exact build and its test tool
$build = Join-Path $OutRoot "build-$short"
gh run download $run.databaseId -R $Repo -n riffhouse-windows -D (Join-Path $build 'installer')
gh run download $run.databaseId -R $Repo -n riffhouse-windows-tests -D (Join-Path $build 'tool')
$setup = Get-ChildItem (Join-Path $build 'installer') -Filter *.exe | Select-Object -First 1
$hash = (Get-FileHash $setup.FullName -Algorithm SHA256).Hash.ToLower()
$sums = Get-Content (Join-Path $build 'installer\SHA256SUMS.txt') -Raw
if ($sums -notmatch $hash) { Say "STOPPED: installer hash $hash is not the one CI wrote"; exit 1 }
Say "Installer $($setup.Name) sha256 $hash (matches CI)"
$tool = (Get-ChildItem (Join-Path $build 'tool') -Filter riffhouse_tests.exe -Recurse | Select-Object -First 1).FullName

# 3. clips: every recording through every tone, captures (new) and circuit models (old)
$clips = Join-Path $OutRoot 'blind'
$keyDir = Join-Path $OutRoot 'key-do-not-open'
New-Item -ItemType Directory -Force -Path $clips, $keyDir | Out-Null
$key = Join-Path $keyDir 'key.csv'
'clip,recording,tone,A,B' | Out-File $key -Encoding utf8
$recordings = foreach ($f in $DiFolder) { Get-ChildItem $f -Filter *.wav -File -ErrorAction SilentlyContinue | Sort-Object Length -Descending | Select-Object -First $MaxRecordings }
if (-not $recordings) { Say 'STOPPED: no .wav recordings found'; exit 1 }
$n = 0; $bad = 0
foreach ($rec in $recordings) {
    foreach ($tone in $Tones) {
        $n++
        $id = '{0:d2}' -f $n
        $newIsA = (Get-Random -Maximum 2) -eq 0
        $a = Join-Path $clips "$id-A.wav"; $b = Join-Path $clips "$id-B.wav"
        $newOut = if ($newIsA) { $a } else { $b }
        $oldOut = if ($newIsA) { $b } else { $a }
        & $tool play $rec.FullName $tone $newOut 2>&1 | Out-File (Join-Path $keyDir 'render.log') -Append
        if ($LASTEXITCODE -ne 0) { $bad++ }
        & $tool play $rec.FullName $tone $oldOut captures=0 2>&1 | Out-File (Join-Path $keyDir 'render.log') -Append
        if ($LASTEXITCODE -ne 0) { $bad++ }
        "$id,$($rec.Name),$tone,$(if ($newIsA) {'new'} else {'old'}),$(if ($newIsA) {'old'} else {'new'})" | Out-File $key -Append -Encoding utf8
    }
}
Say "Rendered $n pairs from $(@($recordings).Count) recordings x $($Tones.Count) tones into $clips ($bad renders failed)"

# 4. how much of this PC's processor the sound engine takes
& $tool cpu 2>&1 | Out-File (Join-Path $OutRoot 'cpu.txt')
Say "CPU check: $(Join-Path $OutRoot 'cpu.txt')"
Say 'DONE'
