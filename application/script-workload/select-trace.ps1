param($tar, $start_trace, $end_trace, $start_event, $end_event)

import-csv $tar\trace-mark.csv | %{
	if ($_.cmd_id -match "([^.]*)")
	{
#		write-host "id=$($_.cmd_id), matches=$($matches[1])"
		[int]$cmd_id = $matches[1];
		if (($start_trace -le $cmd_id) -and ($cmd_id -le $end_trace))
		{
			$_
		}
	}
	
} | export-csv $tar\trace-select.csv

#import-csv $tar\event-mark.csv | ?{
#	($start_event -le $_.cmd_id) -and ($_.cmd_id -le $end_trace)
#} | export-csv $tar\event-select.csv
