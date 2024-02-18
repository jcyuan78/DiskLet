param($dir)
#param($src_fn, $dst_fn, $file_map)

function mark
{
    process
    {
        $item = $_;
        #find file from mapping
        if (($item.cmd -eq "Read") -or ($item.cmd -eq "Write"))
        {
            #$file_info = find-file -lba $item.lba;
            $file_info = $file_map.FindFile([int64]$item.lba);
#            if ($file_info -ne $null)
#            {
                add-member -InputObject $item -MemberType NoteProperty -Name "FID" -Value $file_info.fid;
                add-member -InputObject $item -MemberType NoteProperty -Name "file" -Value $file_info.filename;
#            }
        }
        $item;
    }

}

write-host "loading file map..."
$file_map = .\load-file_mapping.ps1 -src_fn "$dir\filemap.csv"

write-host "processing trace ..." 

import-csv "$dir\trace.csv" | mark |
    select-object cmd_id,time,delta,cmd,SQID,Age,lba,lba_hex,secs,secs_hex,fid,file | 
    export-csv "$dir\trace_fs.csv"