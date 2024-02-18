param ([int64]$cap, [int64]$offset)
begin 
{
	$status = 0		#standby
}
process
{
#	$line
	$line = $_
	if ( $line -eq "")
	{
		return
	}
	if ($line -match "File\s+(\d+)")
	{#enter file process
		$status = 1		#enter file
		[int]$file_id = $matches[1]
		return
	}
	switch ($status)
	{
		"1"	{#file name
			$file_name = $line
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
				$length = $end_lba - $start_lba + 1
				
				$lba_map = new-object psobject -prop (@{
					'start'= ($start_lba + $offset);
					'len'=$length;
					'attr'=$attr;
					'fid'=$file_id;
					'fn'=$file_name}	
				)
				$lba_map
			}
		}
	}
}
end
{
	$lba_map = new-object psobject -prop (@{
		'start'= ($cap + $offset);
		'len'=0;
		'attr'="";
		'fid'= -2;
		'fn'="=END="}
	)
#	$lba_map
}
