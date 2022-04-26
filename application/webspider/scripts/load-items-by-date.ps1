param ($start_date, $end_date)
import-module .\htmlparser.dll
#$start_date = "2021-06-25";
#$end_date = "2021-07-31";

$Global:item_num = 0;

$read=0;
$page=1;

#$table = @();
$fn = [System.String]::Format("Court_{0}_{1}.csv", $start_date, $end_date);

while (1)
{
    write-host "reading page $page, total items=$($Global:item_num), processed items=$read";
    $items = .\load-page.ps1 -start_date $start_date -end_date $end_date -page $page;
    if (($items -eq $null) -or ($items.count -eq 0)) {break;}
    #$table += $items;
    $items | export-csv $fn -Encoding UTF8 -Append;
    $read += ($items.count-1);
    write-debug "total items=$($Global:item_num), read item=$read";
    if ($read -ge $Global:item_num)   {      break;   }
    $page ++;
    $delay = get-random -inputobject (100..200);
    $delay /= 20;
    Write-Host "delay $delay seconds"
    Start-Sleep -Seconds $delay;
}
#$table | export-csv $fn -encoding UTF8;