param($src_fn, $mapping_list, $offset)

#[string]$pattern;
$script:offset = $offset;
#$script:mapping = $mapping;


function offset_to_lba($fn, $detail)
{
    if ($detail -match "Offset: ([0-9,]*), Length: ([0-9,]*)")
    {
        [int64]$file_offset = $Matches[1];
        [int64]$length = $Matches[2];
        [int64]$local_lba = $file_offset / (512*8) ;
        $secs = $length / 512;
        $infile=$true;

        write-debug "processing file $fn";
        $mapping = $file_mapping_list[$fn];
        if ($mapping -ne $null)
        {
            write-debug "found file mapping for $fn";
            $start = $mapping[$local_lba]+$script:offset;
        }
        elseif ($fn -eq "C:") 
        {
            [int64]$start = $local_lba + $script:offset; 
            if (-not ($mapping -contains $start) ) {$infile = $false;}

        }
        New-Object PSObject -property (@{"address" = $start*8; "secs" = $secs; "infile" = $infile})
    }
}


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

        if ($line.Operation -eq "WriteFile")
        {
            $lba = offset_to_lba -fn $($line.path) -detail $($line.detail)
        }
        elseif ($line.Operation -eq "ReadFile")
        {
            $lba = offset_to_lba -fn $($line.path) -detail $($line.detail)
        }
        if ($lba -ne $null)
        {
#            write-host "lba=$($lba.lba), secs=$($lba.secs)"
            $address = $lba.address;
#            write-host "address = $address"
      		add-member -membertype noteproperty -name address -value $address -inputobject $line;
            add-member -MemberType NoteProperty -name secs -Value $($lba.secs) -InputObject $line;
            add-member -MemberType NoteProperty -name infile -Value $($lba.infile) -InputObject $line;
            
#            write-host $line;
            $lba = $null;
        }
        else
        {
      		add-member -membertype noteproperty -name address -value 0 -inputobject $line;
            add-member -MemberType NoteProperty -name secs -Value 0 -InputObject $line;
            add-member -MemberType NoteProperty -name infile -Value $false -InputObject $line;
        }
        $line;

    }
    end
    {

    }


}

#process mapping
write-debug "processing map"
$file_mapping_list = @{};
foreach ($mapping in $mapping_list)
{
    write-debug "processing file $($mapping.fn), length=$($mapping.file_length)";
    $file_mapping = new-object 'int64[]' $($mapping.file_length / 8); 
    $ll=0;
    $ss=0;
    foreach ($segment in $($mapping.segments))
    {
        Write-Debug "processing segment $ss";
        $start_lba = $segment.start;
        $lba_num = $segment.secs;

        for ($jj=0; $jj -lt $lba_num; $jj+=8)
        {
            $file_mapping[$ll] = $start_lba/8;
            $ll++;
            $start_lba++;
        }
        $ss++;
    }
    $file_mapping_list[$mapping.fn] =$file_mapping;
    $file_mapping = $null;
}

#$file_mapping;

#exit;

write-debug "processing event"


import-csv $src_fn | process_event | 
	Select-Object "Time of Day","Process Name","PID","Operation","Path","Result","address","secs","infile","Detail"
 #| Export-Csv event.csv
