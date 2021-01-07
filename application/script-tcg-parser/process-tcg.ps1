param ($tar)

# pre-process, convert txt to bin


# pick up TCG commands
$list_fn = "$tar\list.csv";
$tcg_cmd_fn = "$tar\tcg-cmds.csv";

import-csv $list_fn | .\parse-lecory-cmd | ?{
	($_.cmd -eq "Security Send") -or ($_.cmd -eq "Security Receive")
} | Select-Object time,delta,cmd_id,cmd,CDW10,CDW11,CDW12 | export-csv $tcg_cmd_fn

# decode TCG commands
import-csv $tcg_cmd_fn | %{
    $cmd = $_;
	if ($cmd.cmd -eq "Security Send") 
	{
        $send_cmd = $cmd;
        $send_data = Import-Binary "$tar\binary\cmd-$($cmd.cmd_id)-send.bin";
        $send_packet = ConvertFrom-TcgPayload $send_data -CDW10 $($cmd.CDW10);
        $TSN  = $send_packet.packet.TSN;
        $HSN = $send_packet.packet.HSN;        
        $protocol = ($cmd.CDW10 -band 0xFF000000) / 0x1000000;
	}
	else 
	{
        $receive_cme = $cmd;
        $receive_data = Import-Binary "$tar\binary\cmd-$($cmd.cmd_id)-receive.bin";
        $receive_packet = ConvertFrom-TcgPayload $receive_data -CDW10 $($cmd.CDW10);

        $tcg_cmd = Convert-TcgCmd -send $send_packet.packet.sub_packet.payload -recevi = $receive_packet.packet.sub_packet.payload
        $tcg_cmd.Print();

    }


#	write-host "cmd $($cmd.cmd_id) $($cmd.cmd): protocol=$protocol,  session=($TSN, $HSN)";
#	$sub_packet = $packet.packet.sub_packet;
#	Out-Binary -data $sub_packet.payload;
#	$token = Convert-TcgToken -payload $sub_packet.payload -receive $receive;
#	$token.Print();
}