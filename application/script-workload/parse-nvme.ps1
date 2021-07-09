param($src_fn, $dst_fn)

function parse-cmd
{

    process
    {
        $cmd_in = $_;
		
		if ($cmd_in."Type H->D" -ne "") {$cmd = $cmd_in."Type H->D";}
		elseif ($cmd_in."Type D->H" -ne "") {$cmd = $cmd_in."Type D->H";}
		else {$cmd = "";}
		
		if ( ($cmd -eq "Read") -or ($cmd -eq "Write") -or ($cmd -eq "Flush") )
		{
			[int64]$d10 = $cmd_in.CDW10;
			[int64]$d11 = $cmd_in.CDW11;
			[int32]$d12 = $cmd_in.CDW12;
			[int64]$lba = $d11 * 0x100000000 + $d10;
			$secs = ($d12 -band 0xFFFF) + 1;
			
			if ($cmd_in.Item -match "NVMe Cmd (\d*)") {	$cmd_id = $matches[1];	}
			else {$cmd_id = 0;}
			
			$time = $cmd_in."Time Stamp";
			if ($cmd_in."Time Delta" -match "([\d\.]*) (us|ms)")
			{
				[float] $delta = $matches[1];
				if ($matches[2] -eq "ms") {$delta *=1000;}
			}
			else {$delta = 0;}
			
#			$cmd_out = new-object PSObject
#			add-member -membertype noteproperty -name offset -value $offset -inputobject $line;
			new-object PSObject -property @{
				"cmd_id"=$cmd_id;
				"time"=$time;
				"delta"=$delta;
				"cmd"=$cmd;
				"op_code"=0;
				"lba"=$lba;
				"secs"=$secs;
			}
        }
    }

}

import-csv $src_fn | parse-cmd | select-object cmd_id,time,delta,cmd,op_code,lba,secs | export-csv $dst_fn
