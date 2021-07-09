param($src_fn, $dst_fn)

# 将LeCroy的时间格式解析为浮点数，unit表示单位，0:s，1:ms, 2:us, 3:ns
function analyze_time($str, $unit=0)
{
    $reg_tt = [regex] "(\d+)\.?";
    $reg_vv = [regex] "($reg_tt)* \((min|s|ms|us|ns)\)";

    write-debug "source = $str"
    $res = $reg_vv.Matches($str);
    write-debug "res = $res"    
    $str_val = $res.groups[2].captures;

    $uu=$res.groups[3].captures.value;
    [float]$time = 0;
    $ptr = 0;

    switch ($uu)
    {
        #分钟和其他单位的进制不同，先转换为秒
        "min" {$time += [float]($str_val[$ptr].value) * 60;
            $ptr ++;    $exp = 0;     }
        "s"  {$exp=0;}
        "ms" {$exp=1;}
        "us" {$exp=2;}
        "ns" {$exp=3;}
    }
    for (;$ptr -lt $str_val.Count; $ptr ++)
    {
        $time += [float]($str_val[$ptr].value);
        $time *=1000;        
        $exp++;        
    }
    for (;$exp -gt $unit; $exp--)  {  $time /=1000;  }
    $time;
}

filter parse_lecroy
{
    begin
    {
        $line_id = 0; #skip several lines
        $reg_seg = [regex] "([^|]+),"
        $reg_line = [regex] "($reg_seg)*"
        $cmd_id =0;
        $next_update =0;
    }
    process
    {
        $line = $_
        $line_id ++;

#        if ($line_id -gt 9)
#        {
            $res = $reg_line.Matches($line);
            write-debug "got line $line_id, res_num = $($res.Count)"
            if ($res.Count -ge 15)
            {
                $str_frame = $res[8].Captures.Value;
                Write-Debug "frame = $str_frame";
                if ($str_frame -match "FIS 27")
                {
                    write-debug "parsing line"
                    # 分析time stamp
                    write-debug "line = $line_id"
                    $str_time_stamp = $res[0].Captures.Value;
                    $time_stamp = analyze_time $str_time_stamp;
                    #$cmd_id
                    $str_delta = $res[4].Captures.Value;
                    $delta = analyze_time $str_delta -unit 2;

                    $str_cmd = $res[10].Captures.Value;
                    if ($str_cmd -match "(0x[0-9A-Z]+)")
                    {
                        $op_code = [int]$matches[1];
                        switch ($op_code)
                        {
                            0x60 {$cmd = "Read";}
                            0x61 {$cmd = "Write";}
                            0xEA {$cmd = "Flush";}
                            0x06 {$cmd = "Trim";}
                            default {$cmd = "Unkwon";}
                        }
                    }
                    if (($cmd -eq "Read") -or ($cmd -eq "Write"))
                    {
                        $str_lba = $res[12].Captures.Value;
                        $lba = [int64]"0x$str_lba";
                        $str_secs = $res[14].Captures.Value;
                        if ($str_secs -match "[0-9A-Z]+\s\-\s([0-9A-Z]+)")
                        {
                            $secs = [int64]"0x$($matches[1])";
                        }
                    }
                    else {
                        $lba = -1;
                        $secs = 0;
                    }

                    New-Object PSObject -Property @{
                        "time"=$time_stamp;
                        "delta"=$delta;
                        "cmd"=$cmd;
                        "op_code"=$op_code;
                        "lba"=$lba;
                        "secs"=$secs;
                    }
                    $cmd_id ++;
                    if ($cmd_id -gt $next_update)
                    {
                        write-host "processed line=$line_id, commands=$cmd_id";
                        $next_update+=100;
                    }
                }
            }
#        }
    }    
}

#get-content $src_fn -totalcount 14 | parse_lecroy

#get-content $src_fn -totalcount 50 | parse_lecroy | select-object cmd_id,time,delta,cmd,op_code,lba,secs | export-csv $dst_fn
#get-content $src_fn | parse_lecroy | select-object time,delta,cmd,op_code,lba,secs | export-csv $dst_fn

$files = dir "$src_fn*.csv";
$files.FullName | %{
    write-debug "parsing $_";
    get-content $_} | parse_lecroy | select-object time,delta,cmd,op_code,lba,secs | export-csv $dst_fn
#analyze_time $src_fn
