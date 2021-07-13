param ($ssd, $saturation)

$disk_info = get-diskinfo;
$capcity = $disk_info.capacity;

[int64]$fill_size = [int64]($capcity * $saturation);
invoke-simulate $ssd -cmd "Write" -lba 0 -secs $fill_size;
