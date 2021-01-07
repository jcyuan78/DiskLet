param($fn)

function ToBinary
{
    begin
    {
        $reg_hex = [regex] "[a-zA-Z0-9]";
#        write-debug $regex_hex;
#        write-debug $regex_hex.ToString();
        $reg_add = [regex] "($reg_hex{5}):";
        $reg_byte = [regex] "$reg_hex{2}";
        $reg_line = [regex] "$reg_add\s*(($reg_byte)\s*)*";
#        write-debug $reg_add;
        $array = New-Object System.Collections.ArrayList;
    }
    process
    {
        $line = $_;
        $res = $reg_line.Matches($line);
        write-debug $res;
        if ($res -ne $null)
#        if ($line -match "$($reg_add.ToString())" )
        {
            $offset = $res.groups[1].Captures.Value;
            write-debug "offset = $offset"; 
            foreach ($dd in $res.groups[3].Captures)
            {
                $array += [int]("0x$($dd.value)");
                write-debug "array size = $($array.count)";
            }    
        }
    }
    end
    {
        $array | ConvertTo-Binary;
    }
}

get-content $fn |  ToBinary;
#| export-binary "$fn.bin"