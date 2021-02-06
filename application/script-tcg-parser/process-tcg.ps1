param ($tar)

# pre-process, convert txt to bin


# pick up TCG commands
#$list_fn = "$tar\list.csv";
$tcg_cmd_fn = "$tar\security.csv";

#import-csv $list_fn | .\parse-lecory-cmd | ?{
#	($_.cmd -eq "Security Send") -or ($_.cmd -eq "Security Receive")
#} | Select-Object time,delta,cmd_id,cmd,CDW10,CDW11,CDW12 | export-csv $tcg_cmd_fn

# decode TCG commands
import-csv $tcg_cmd_fn | %{
    $cmd = $_;
    Write-debug "processing command, id=$($cmd.cmd_id), cmd=$($cmd.cmd)"
	if ($cmd.cmd -eq "Security Send") 
	{
        $send_cmd = $cmd;
        $receive = $false;
	}
	else 
	{
        $receive_cmd = $cmd;
        $receive = $true;
    }


    $data = Import-Binary "$tar\binary\cmd-$($cmd.cmd_id).bin";
    $protocol = ($cmd.CDW10 -band 0xFF000000) / 0x1000000;
    $comid = ($cmd.CDW10 -band 0xFFFF00) / 0x100;

    $packet = ConvertFrom-TcgPayload $data -CDW10 $($cmd.CDW10)  -receive $receive;   
    write-debug "protocol=$($protocol.ToString('X02')), comid=$($comid.ToString('X04')), type=$($packet.GetType().Name)";
 

 #   write-host "cmd_id=$($cmd.cmd_id), cmd=$($cmd.cmd), protocol=$($protocol.ToString('X02')), comid=$($comid.ToString('X04'))"

    [System.String]::Format("cmd_id={0}, cmd={1}, protocol={2:X02}, comid={3:X04}", 
        $cmd.cmd_id, $cmd.cmd, $protocol, $comid);
    if ($packet.getType().name -eq "ComPacket")
    {
        $TSN  = $packet.packet.TSN;
        $HSN = $packet.packet.HSN;  
        write-debug "TSN = $TSN, HSN=$HSN";
#	write-host "cmd $($cmd.cmd_id) $($cmd.cmd): protocol=$protocol,  session=($TSN, $HSN)";
    	$sub_packet = $packet.packet.sub_packet;
#	Out-Binary -data $sub_packet.payload;
    	$token = Convert-TcgToken -payload $sub_packet.payload -receive $receive;
        $token.Print();
    }
    else 
    {
        $packet.features;
    }
#    elseif ($packet.getType().name -eq "L0Discovery")
#    {

 #   }
}