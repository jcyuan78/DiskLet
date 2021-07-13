param($tar, [int64]$offset=0, [int64]$cap=0, $index=0)

#Import-Module ..\DEBUG_DYNAMIC\workload-analyzer.dll
#size=103534592 secs, offset=1320960 secs
#ecs   56000000 -offset 0
#cd .\wla\

#write-host "parsing trace"
#.\parse-bushund.ps1 $tar\trace.txt $tar\trace.csv

#read cap and offset from file-mapping
$line = get-content $tar\file-mapping.txt | select-object -first 1;
write-debug "first line in file-mapping.txt = $line"
if ($line -match "size=(\d*) secs, offset=(\d*) secs")
{
	$cap = $matches[1];
	$offset = $matches[2];
	write-debug "got disk info from file: cap=$cap, offset=$offset"
}


write-host "parsing file mapping..."
$file_mapping = .\parse-mapping.ps1 $tar\file-mapping.txt -offset $offset
$file_mapping | Export-Clixml $tar\file-mapping.xml

write-host "parsing event file..."
.\parse-event.ps1 $tar\event.CSV | export-csv $tar\event-new.csv

write-host "loading disk and file mapping..."
Import-Csv $tar\disk-mapping.csv | Set-StaticMapping -secs $($cap+$offset) -first_lba $offset
Add-FileMapping -fn $file_mapping[$index].fn -segments $file_mapping[$index].segments -first_lba $offset

write-host "marking trace..."
Import-Csv $tar\trace.csv | Convert-Trace | Select-Object cmd_id,time,delta,cmd,op_code,lba,secs,offset,fid,fn | export-csv $tar\trace-mark.csv
write-host "marking event..."
import-csv $tar\event-new.csv | convert-event -first_lba $offset | select-object "Time of Day","Operation","lba","secs","offset","fid","Path","Detail" | export-csv $tar\event-mark.csv



