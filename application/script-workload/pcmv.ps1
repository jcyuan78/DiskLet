param([string]$tar)
#get disk size
$part = "ASSOCIATORS OF {Win32_LogicalDisk.DeviceID='C:'} " +
		"WHERE AssocClass = Win32_LogicalDiskToPartition"
$partition = Get-WmiObject -query $part
$offset = $partition.StartingOffset / 512;
$cap = $partition.Size / 512;
write-host "offset=$offset; size=$cap"
[system.string]::Format("size={0} secs, offset={1} secs", $cap, $offset) > $tar

#& ".\nfi.exe" "c:" >> "all-mapping.txt"
#write-host "Ready for test..."

while (1)
{
    $(get-date) >> $tar
    dir C:\PCMark*.tmp | %{
        $file = $_;
        & ".\nfi.exe" $file.FullName >> $tar
    }
    start-sleep -Milliseconds 400

}