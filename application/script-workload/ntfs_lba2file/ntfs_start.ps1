param(
	[string]$drive, 
	[string]$out_fn = "file.csv")
#get disk size
$part = "ASSOCIATORS OF {Win32_LogicalDisk.DeviceID='$drive'} " +
		"WHERE AssocClass = Win32_LogicalDiskToPartition"
$partition = Get-WmiObject -query $part
$offset = $partition.StartingOffset / 512;
$cap = $partition.Size / 512;
write-host "offset=$offset; size=$cap"

#& ".\nfi.exe" $drive | & ".\lba2file.ps1" -offset $offset -cap $cap |
#	sort-object start | & ".\fill_unused.ps1" | select-object start,len,attr,fid,fn | export-csv $out_fn
	
	
& ".\nfi.exe" $drive | & ".\lba2file.ps1" -offset $offset -cap $cap |
	select-object start,len,attr,fid,file_offset,fn | export-csv $out_fn
