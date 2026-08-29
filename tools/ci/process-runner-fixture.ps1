param([string]$Text='', [string]$ErrorText='', [int]$Bytes=0, [int]$ExitCode=0, [int]$SleepSeconds=0)
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
[Console]::InputEncoding = [Text.UTF8Encoding]::new($false)
if($SleepSeconds){Start-Sleep -Seconds $SleepSeconds}
if($Text){[Console]::Out.WriteLine($Text)}
if($Bytes){[Console]::Out.Write(('x' * $Bytes))}
if($ErrorText){[Console]::Error.WriteLine($ErrorText)}
exit $ExitCode
