process
{
    $cmd_in = $_;
		
	if ($cmd_in."Type H->D" -ne "") {$cmd = $cmd_in."Type H->D";}
	elseif ($cmd_in."Type D->H" -ne "") {$cmd = $cmd_in."Type D->H";}
	else {$cmd = "";}
		
	[uint32]$d10 = $cmd_in.CDW10;
	[uint32]$d11 = $cmd_in.CDW11;
	[uint32]$d12 = $cmd_in.CDW12;
			
	if ($cmd_in.Item -match "NVMe Cmd (\d*)") {	$cmd_id = $matches[1];	}
	else {$cmd_id = 0;}
			
	$time = $cmd_in."Time Stamp";
	if ($cmd_in."Time Delta" -match "([\d\.]*) (us|ms)")
	{
		[float] $delta = $matches[1];
		if ($matches[2] -eq "ms") {$delta *=1000;}
	}
	else {$delta = 0;}
		
	new-object PSObject -property @{
		"cmd_id"=$cmd_id;
		"time"=$time;
		"delta"=$delta;
		"cmd"=$cmd;
		"CDW10"=$d10;
		"CDW11"=$d11;
		"CDW12"=$d12;
	}
}


