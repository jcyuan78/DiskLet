param ($tar, $trace)

$ssd = New-Drive -name "hot_index" -config "$pwd\nand-config.json"
Start-Simulate $ssd
$ssd.EnableHotIndex(0);
#.\script-workload\prefill.ps1 $ssd 0.7
#$ssd
.\script-workload\fill-predata.ps1 $ssd "$tar\file.csv"
$ssd
#$ssd.EnableHotIndex(1);
import-csv "$tar\$trace" | Invoke-Simulate $ssd
$ssd
