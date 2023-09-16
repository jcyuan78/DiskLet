param($src_fn, $dst_fn)

$cmd_id = 0;
function parse_cmd {
    process   {
        $src_item = $_;
        write-debug "parse cmd $($src_item.down), lba = $($src_item.LBA)";
        $cmd = $src_item.down;
        $lba = 0;
        $secs = 0;
        if (($cmd -eq "Read") -or ($cmd -eq "Write") )
        {
            $lba = [int64]"0x$($src_item.LBA)";
            $secs = [int64]"0x$($src_item.'Expected Exchange Data Bytes')" / 0x200;
        }
        $cmd_id ++;
        $SQID = -1;
        try {
            $SQID = [int]"0x$($src_item.SQID)";
        } catch {
            write-host "convert error, SQID=$($src_item.SQID), err= $_";
        }


        new-object PSObject -property @{
            "cmd_id" = $cmd_id;
            "cmd"=$src_item.down;
            "time"=$src_item."mm:ss.ms_us_ns_ps";
            "delta"=$src_item."Delta Time";
            "lba" = $lba;
            "lba_hex" = $lba.ToString("X");
            "secs" = $secs;
            "secs_hex" = $secs.ToString("X")
            "SQID" = $SQID;
            "Age" = $src_item."Age(us)"
        }
    }
}

import-csv $src_fn | parse_cmd | 
    select-object cmd_id,time,delta,cmd,SQID,Age,lba,lba_hex,secs,secs_hex | 
    export-csv $dst_fn;