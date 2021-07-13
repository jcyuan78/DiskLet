param($src, $dst)

function convert-to-cluster
{
    begin {
        $line_id = 0;
        $next_update = 0;
    }
    process 
    {
        $line_id ++;
        $cmd = $_;
        $start_cluster = [int] (($cmd.lba -band 0xFFFFFFF8) / 8);
        $cluster_num = [int] ($cmd.secs-1 /8)+1;
        for ([int]$sec = 0; $sec -lt $cmd.secs; $sec += 8 )
        {
            New-Object psobject -Property @{
                "cmd_id"=$cmd.cmd_id;
                "delta" =$cmd.delta / $cluster_num;
                "cmd" = $cmd.cmd;
                "cluster" = $start_cluster; 
                "offset" = [int]($cmd.offset) + $sec*8;
                "fid" = $cmd.fid;
                "fn" = $cmd.fn;
            }
            $start_cluster ++;
        }

        if ($line_id -le $next_update)
        {
            Write-Progress -Status "Parsing line=$line_id" -Activity "Parsing trace..." -PercentComplete 10
            $next_update+=100;            
        }
    }
}


import-csv $src | convert-to-cluster | select-object cmd_id,delta,cmd,cluster,offset,fid,fn | export-csv $dst
