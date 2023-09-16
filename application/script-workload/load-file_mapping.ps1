param($src_fn)

$file_mapping = new-filemap;

#function add_item {
#    process {
#        $item = $_;
#        add-filemapping -start [int64]$item.start -secs [int64]$item.len -attr $item.attr -fid [int]$item.fid -filename $item.fn;
#    }
#}


import-csv $src_fn | %{
    $file_mapping.AddSegment([int64]$_.start, [int64]$_.len, $_.attr, [int]$_.fid, $_.fn);
}

$file_mapping;