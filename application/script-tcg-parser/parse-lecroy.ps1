param($tar)

function parse-cmd
{
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
}

$list_fn = "$tar\list.csv";

import-csv $list_fn | parse-cmd | ?{
	($_.cmd -eq "Security Send") -or ($_.cmd -eq "Security Receive")
} | %{
	$cmd = $_;
	if ($cmd.cmd -eq "Security Send") 
	{
		$fn = "cmd-$($cmd.cmd_id)-send.bin";
		$receive = $false;
	}
	else 
	{
		$fn = "cmd-$($cmd.cmd_id)-receive.bin";
		$receive = $true;
	}
	$protocol = ($cmd.CDW10 -band 0xFF000000) / 0x1000000;
	write-host "cmd $($cmd.cmd_id) $($cmd.cmd): protocol=$protocol";

	$data = import-binary "$tar\binary\$fn";
	if ($data -ne $null)
	{
		$packet = ConvertFrom-TcgPayload $data -CDW10 $($cmd.CDW10);
		if ( ($packet -ne $null) -and ($packet.packet.sub_packet.payload -ne $null) )
		{
			$TSN  = $packet.packet.TSN;
			$HSN = $packet.packet.HSN;
			write-host "session=($TSN, $HSN)";
			$sub_packet = $packet.packet.sub_packet;
			Out-Binary -data $sub_packet.payload;
			$token = Convert-TcgToken -payload $sub_packet.payload -receive $receive;
			$token.Print();
		}
	}
}


