#Push-Location

if (-not(Test-Path .\temp))
{
    mkdir .\temp;
}
rm .\temp\* -Force;

write-host "Input start date, <enter> for today:"
$ss = read-host;
if ($ss -eq "")
{
    $ss = get-date -format "yyyy-MM-dd";
}
Write-Host "Input end date, <enter> 2 months:"
$ee = Read-Host;
if ($ee -eq "")
{
    $dd =(get-date).AddMonths(2);
    $ee = $dd.ToString("yyyy-MM-dd");
}

Write-Debug "start from $ss to $ee";

#cd "$home\Desktop\fwupdate"
$powershell = "$($env:SystemRoot)\system32\WindowsPowerShell\v1.0\powershell.exe"
Start-Process $powershell -ArgumentList "-file","$pwd\load-items-by-date.ps1 -start_date $ss -end_date $ee","-ExecutionPolicy","Unrestricted" -Verb runas -Wait;
