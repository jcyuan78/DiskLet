param ($content)

$doc = $content | ?{$_ -ne ""} | ConvertFrom-html;
#$doc = Get-Content .\table.html | ?{$_ -ne ""} | ConvertFrom-html;
$root = $doc.GetRoot();
$tables = $root.FindAllTags("TBODY");
$rows = $tables[0].FindAllTags("TR");
# get title of the table

foreach ($row in $rows) {
    $cols = $row.FindAllTags("TD");
    $data = New-Object PSObject;
    for ($cc=0; $cc -lt $cols.Count; $cc++)
    {
        $vv = $cols[$cc].text;
        $val = $vv.replace("&nbsp;", " ");
#        Write-Debug "vv=$vv, val = $val";

        switch ($cc)
        {
            0 {$name = "Court";}
            1 {$name = "SubCourt";}
            2 {$name = "Date";}
            3 {$name = "Id";}
            4 {$name = "Reason";}
            5 {$name = "Department";}
            6 {$name = "Judge";}
            7 {$name = "Plaintiff";}
            8 {$name = "Defendant";}
            9 {$name = "Type";}
        }
        $data | Add-Member -NotePropertyName $name -NotePropertyValue $val;
    }
    $data;
}

#查找总条数
$divs = $doc.FindAllTags("DIV")
$div = $divs[$divs.count-1];
if ( $div.text -match "共有数据(\d+)条" )
{
    $item_num = [int]($matches[1]);
    Write-Debug "item number = $item_num";
    if ( ($global:item_num -eq $null) -or ($global:item_num -eq 0))
    {
        $global:item_num = $item_num;
    }
}
