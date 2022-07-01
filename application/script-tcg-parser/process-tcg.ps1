param ($tar=$global:tar, $cmd_fn="command.csv")

# pre-process, convert txt to bin

. .\pre-process-list.ps1
# pick up TCG commands
#$list_fn = "$tar\list.csv";
$tcg_cmd_fn = "$tar\$cmd_fn";

#import-csv $list_fn | .\parse-lecory-cmd | ?{
#	($_.cmd -eq "Security Send") -or ($_.cmd -eq "Security Receive")
#} | Select-Object time,delta,cmd_id,cmd,CDW10,CDW11,CDW12 | export-csv $tcg_cmd_fn

# decode TCG commands
import-csv $tcg_cmd_fn | parse-cmd | ?{
	($_.cmd -eq "Security Send") -or ($_.cmd -eq "Security Receive")} | %{

    $cmd = $_;
    Write-debug "processing command, id=$($cmd.cmd_id), cmd=$($cmd.cmd), CDW10=$($cmd.CDW10)"
	if ($cmd.cmd -eq "Security Send") 
	{
        $send_cmd = $cmd;
        $receive = $false;
	}
	elseif ($cmd.cmd -eq "Security Receive")
	{
        $receive_cmd = $cmd;
        $receive = $true;
    }
	$cc = [int]($cmd.cmd_id);
	$data_index = $cc.ToString("D03");
#	write-debug "cmd id = $($cmd.cmd_id) = $data_index";
	$protocol = ($cmd.CDW10 -band 0xFF000000) / 0x1000000;
	$comid = ($cmd.CDW10 -band 0xFFFF00) / 0x100;
	
	write-host "processing command: $cc, $($cmd.cmd), protocol=$($protocol.ToString("X02")), comid=$comid " -ForegroundColor Yellow
	"processing command: $cc, $($cmd.cmd), protocol=$($protocol.ToString("X02")), comid=$comid "

	try	{
		$data = Import-Binary "$tar\binary\cmd-$data_index.bin";

	#    $packet = ConvertFrom-TcgPayload $data -CDW10 $($cmd.CDW10)  -receive $receive;   
	#    write-debug "protocol=$($protocol.ToString('X02')), comid=$($comid.ToString('X04')), type=$($packet.GetType().Name)";
		$obj = Convert-SecurityCmd -payload $data -cdw10 $($cmd.CDW10) -receive $receive -nvme_id $cc;
		$obj.ToString("", $null);
	}
	catch
	{
		write-host "failed on parsing command $cc" -ForegroundColor Red
	}
}