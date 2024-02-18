# This script is used to increase the version in a vc resource file
param($res_fn)
#convert event.csv file, parse the detail field


function process_resource
{

    begin 
    {
        $line_id = 0;
    }
    process
    {
        $line = $_;
        $line_id ++;
#		write-debug "line=<$line>"
		if ($line -match "FILEVERSION\s*(\d+)\,(\d+)\,(\d+)\,(\d+)" )
		{
			[int]$v1 = $matches[1];
			[int]$v2 = $matches[2];
			[int]$v3 = $matches[3];
			[int]$v4 = $matches[4];
			write-debug "cur ver=$line, $v1, $v2, $v3, $v4"
			$v3 ++;
			
			$line = [String]::Format(" FILEVERSION {0},{1},{2},{3}", $v1,$v2,$v3,$v4);
			write-debug "new ver=$line"
		}
		$line;
		

    }
    end
    {

    }


}

$bak_fn = "$res_fn.bak"

if (test-path $bak_fn) {rm $bak_fn -force;}
mv $res_fn $bak_fn;
get-content $bak_fn | process_resource > $res_fn 
