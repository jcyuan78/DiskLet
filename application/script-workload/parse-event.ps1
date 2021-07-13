param($src_fn)
#convert event.csv file, parse the detail field


function process_event
{

    begin 
    {
        $line_id = 0;
	    $status = 0		#standby
        $file_map = $null;
        $lba = $null;
    }
    process
    {
        $line = $_;
        $line_id ++;
		$offset = 0;
		$secs = 0;

        if ( ($line.Operation -eq "WriteFile") -or ($line.Operation -eq "ReadFile") )
        {
#            $lba = offset_to_lba -fn $($line.path) -detail $($line.detail)
			if ($line.Detail -match "Offset: ([\d,]+), Length: ([\d,]+)")
			{
				[uint64] $offset = $matches[1];
				$offset /= 512;
				[uint64] $secs = $matches[2];
				$secs /= 512;
			}
			else
			{
				write-warning "unmatched detail line:$line_id, detail:$($line.Detail)";
			}

   		add-member -membertype noteproperty -name offset -value $offset -inputobject $line;
        add-member -MemberType NoteProperty -name secs -Value $secs -InputObject $line;
        $line;
        }
    }
    end
    {

    }
}

function process_read_write
{
    begin 
    {
        $line_id = 0;
	    $status = 0		#standby
        $file_map = $null;
        $lba = $null;
    }
    process
    {
        $line = $_;
        $line_id ++;
		$offset = 0;
		$secs = 0;

        if ( ($line.Operation -eq "WriteFile") -or ($line.Operation -eq "ReadFile") )
        {
#            $lba = offset_to_lba -fn $($line.path) -detail $($line.detail)
			if ($line.Detail -match "Offset: ([\d,]+), Length: ([\d,]+)")
			{
				[uint64] $offset = $matches[1];
				$offset /= 512;
				[uint64] $secs = $matches[2];
				$secs /= 512;
			}
			else
			{
				write-warning "unmatched detail line:$line_id, detail:$($line.Detail)";
			}

       		add-member -membertype noteproperty -name offset -value $offset -inputobject $line;
            add-member -MemberType NoteProperty -name secs -Value $secs -InputObject $line;
            $line;
        }
    }
    end
    {

    }
}

function sort_file
{
    process
    {

    }
}

#process mapping
write-debug "processing map"

#exit;

write-debug "processing event"

<#
import-csv $src_fn | process_event | 
#	Select-Object "Time of Day","Process Name","PID","Operation","offset","secs","Detail","Path","Result" | 
	Select-Object "Time of Day","Process Name","PID","Operation","offset","secs","Path","Result" | 
    ?{($_.Operation -eq "CreateFile") -and ($_.path -match "^C\:\\PCMark_10_Storage_2021-5-12")} |
 #   select-object "Time of Day","Process Name","PID","Operation","offset","sec","Path", "Result" |
#    export-csv files.csv
    group-object path
#>


import-csv $src_fn | process_event | 
    ?{($_.path -match "^C\:\\PCMark_10_Storage_2021-5-12")} |
	Select-Object "Time of Day","Process Name","PID","Operation","offset","secs","Detail","Path","Result" | 
 #   select-object "Time of Day","Process Name","PID","Operation","offset","sec","Path", "Result" |
#    export-csv files.csv
#    group-object path
     Export-Csv trace.csv
