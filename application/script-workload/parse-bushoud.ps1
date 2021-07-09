param($src_fn, $dst_fn)

#[string]$pattern;

#$rx_byte = [regex]"([0-9a-f]{2})\s*"

function parse-cmd
{

    process
    {
        $line = $_;
        if ($line -match "CMD\s*(.*)(WRITE|READ)\s*(\S*)\s*(\d*\.\d*\.\d*)\s*(\S*)")
        {
            $param = $Matches[1];
            $cmd = $Matches[2];
			$delta=$Matches[3];
            $cmd_id = $Matches[4];
			$time = $Matches[5];
			
 #           write-host "cmd=$cmd($cmd_id): $param";

            if ($param -match "([0-9a-f]{2})\s*([0-9a-f]{2})\s*([0-9a-f]{2})\s*([0-9a-f]{2})\s*([0-9a-f]{2})\s*([0-9a-f]{2})\s*([0-9a-f]{2})\s*([0-9a-f]{2})\s*([0-9a-f]{2})")
            {
                $cmd_code = $Matches[1];
                $lba0 = $Matches[3];
                $lba1 = $Matches[4];
                $lba2 = $Matches[5];
                $lba3 = $Matches[6];

                $secs0=$Matches[8];
                $secs1=$Matches[9];

                $lba = ((([int]"0x$lba0" * 256) + [int]"0x$lba1") * 256 + [int]"0x$lba2") * 256 + [int]"0x$lba3";
                $secs = [int]"0x$secs0"*256 + [int]"0x$secs1";

            }
			new-object PSObject -property @{
				"cmd_id"=$cmd_id;
				"time"=$time;
				"delta"=$delta;
				"cmd"=$cmd;
				"op_code"=$cmd_code;
				"lba"=$lba;
				"secs"=$secs;
			}
#            [string]::Format("$cmd_id,$cmd,$cmd_code,{0},{1}", $lba, $secs);


        }
    }

}

Get-Content $src_fn | parse-cmd | select-object cmd_id,time,delta,cmd,op_code,lba,secs | export-csv $dst_fn
