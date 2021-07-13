param ($ssd, $file_mapping)

import-csv $file_mapping | %{
    $segment = $_;
    [int64]$lba = $segment.start;
    [int64]$secs = $segment.len;
 #   write-debug "line=$segment, lba=$lba, secs=$secs"
    invoke-simulate $ssd -cmd "Write" -lba $lba -secs $secs;
}