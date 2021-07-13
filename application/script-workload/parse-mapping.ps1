param($src_fn, [int64]$offset)

#[string]$pattern;



function parse-mapping
{

    begin 
    {
	    $status = 0		#standby
        $file_map = $null;
        $pre_seg = "";
        $pre_fn = "";
    }
    process
    {
	    $line = $_
	    if ( $line -eq "")
	    {
		    return
	    }
        if ($line -match "October")
        { # timestamp = reset
            $timestamp=$line;
        }
	    elseif ($line -match "(.*\.tmp)")
	    {#enter file process
		    $status = 1		#enter file
		    $file_name = $line;
            if ($file_name -ne $pre_fn)
            {
                if ($file_map -ne $null)
                {
                    $file_map;
                    $file_map = $null;
                }
                $pre_fn = $file_name;
            }
            $segments = New-Object System.Collections.ArrayList;
            $str_seg="";
		    return;
	    }
	    switch ($status)
	    {
		    "1"	{#file name
			    $status = 2
		    }
		    "2" {#attributes
			    if ($line -match "(\$\w+)\s+\((\w+)\)")
			    {
				    $attr = $matches[1]
			    }
			    elseif ($line -match "logical sectors\s+(\d+)-(\d+)")
			    {
				    [int64]$start_lba = $matches[1]
				    [int64]$end_lba = $matches[2]
#write-debug "get segment from $start_lba to $end_lba"
				    $length = $end_lba - $start_lba + 1;
                    $str_seg += [String]::Format("({0};{1})", $start_lba, $length);
                    $segment = New-Object PSObject -Property (@{"start"=$start_lba+$offset; "secs"=$length});
                    $segments.add($segment) | out-null;
                    $file_length += $length;
			    }
                elseif ($line -match "October")
                {
                    $file_map = New-Object psobject -Property (@{
                        'timestamp' = $timestamp;
                        'fn'="$file_name";
                        'segments'=$segments;
                        'file_length'=$file_length;
                    })
                    $timestamp = $line;
                }
		    }
	    }
    }
    end
    {
        if ($file_map -ne $null) { $file_map;}
	    $lba_map = new-object psobject -prop (@{
		    'start'= ($cap + $offset);
		    'len'=0;
		    'attr'="";
#		    'fid'= -2;
		    'fn'="=END="}
	    )
	    $lba_map
    }


}

<#
$map_list = @{};

Get-Content $src_fn | parse-mapping | %{
	$map_list[$_.fn] = $_;
}
$map_list
#>

Get-Content $src_fn | parse-mapping
