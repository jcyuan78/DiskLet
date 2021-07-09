param($src_fn, $dst_fn)

#[string]$pattern;

#$rx_byte = [regex]"([0-9a-f]{2})\s*"


function TextToBinary ($text)
{
    $reg_hex = [regex] "[a-zA-Z0-9]";
#    $reg_add = [regex] "($reg_hex{5}):";
    $reg_byte = [regex] "$reg_hex{2}";
    $reg_line = [regex] "(($reg_byte)\s*)*";

    $array = New-Object System.Collections.ArrayList; 
    $res = $reg_line.Matches($text);
    write-debug $res;
    if ($res -ne $null)
    {
#        $offset = $res.groups[1].Captures.Value;
#        write-debug "offset = $offset"; 
        foreach ($dd in $res.groups[2].Captures)
        {
            $array += [int]("0x$($dd.value)");
            write-debug "array size = $($array.count)";
        }    
    }  
#    Write-Host "decoded = $array"; 
    $array;
}

function parse-cmd
{
    begin {}
    process
    {
        $line = $_;
        if ($line.length -lt 103) {return;}
#        $reg_line = [regex] "([0-9]{2})?\s*([A-Z]+)?\s*($reg_data)?\s*"
        $str_dev = $line.substring(0, 6).trim();
        $str_phase = $line.substring(8, 5).trim();
        $str_data = $line.substring(15, 50).trim();
        $str_desc = $line.substring(67, 16).trim();
        $str_rep = $line.substring(85, 18).trim();
 
        if ( ($str_dev -ne "") -and ($str_phase -ne ""))
        {# start of pahse
#            write-host "new pahse: dev=$str_dev, phase=$str_phase, data=$str_data, desc=$str_desc, rep=$str_rep";
            # 解析cmd id / phase id / offset
            $reg_rep = [regex] "(\d+).(\d+).(\d+)";
            $res = $reg_rep.Match($str_rep);
            $cmd_id = [int]($res.Groups[1].Value);
            $phase_id = [int]($res.Groups[2].Value);

            if ($null -ne $cur_phase) {
#                write-host "prev phase: $cur_phase"
                $cur_phase;
                $cur_phase = $null;
            }
            if ($null -ne $array ) { $array = $null; }
            $array = New-Object System.Collections.ArrayList;
            $cur_phase = New-Object PSObject -property @{
                "device_id" = [int]$str_dev;
                "phase_type"= $str_phase;
                "description"=$str_desc;
                "data_array" = $array;
                "cmd_id" = $cmd_id;
                "phase_id" = $phase_id;
            }


#            write-host "cur phase: $cur_phase";
        }
        if ($str_data -ne "")
        { #add data to pahse
            $array = TextToBinary -text $str_data;
#            Write-Host "data = $array";
            $cur_phase.data_array += $array;
#            TextToBinary -array $($cur_phase.data_array) -text $str_data;
        }
        
    }
    end{
        if ($null -ne $cur_phase)
        {
            $cur_phase;
        }
    }

}

Get-Content $src_fn | parse-cmd | %{
    $data = $_.data_array | ConvertTo-Binary;
    Add-Member -name data -Value $data -InputObject $_ -MemberType NoteProperty;
    $_;
}
