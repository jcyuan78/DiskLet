param ($tar)

#$dd = Import-Binary C:\Users\MicronFae\test-result\2210\FA\security-payload\Padme3-2210-WO-step03\binary\cmd-197-send.bin
$dd = Import-Binary $tar;
write-host "data:"
$dd |Out-Binary  
$Global:data = $dd;

$pp = ConvertFrom-TcgPayload $dd;
write-host "ComPackage:"
$pp;
write-host "Package:"
$pp.packet;
$pp.packet.payload | Out-Binary;

$ss = $pp.packet.sub_packet;
write-host "SubPacket:"
$ss;
$ss.payload | Out-Binary;


$Global:packet = $pp;
$Global:sub = $ss;

$global:token = Convert-TcgToken -payload $ss.payload;
$global:token.Print();

