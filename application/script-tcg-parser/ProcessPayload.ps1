[CmdletBinding()]
param (
    $tar
)

if (-not (Test-path $tar\text))
{
    mkdir $tar\text
    mv $tar\cmd* $tar\text\
}

if ( -not (Test-Path $tar\binary) )
{
    mkdir $tar\binary;
}

$files = dir $tar\text;
foreach ($file in $files)
{
    write-debug "processing file $($file.FullName)"
    $data= .\ConvertTextToBinary.ps1 $file.FullName;

    if ($file.Name -match "cmd-(\d+)-.*")
    {
        $no=$matches[1];
        $fn = "cmd-$no.bin"
    }
    else {
        $fn = "$($file.Name).bin"
    }

    $data | Export-Binary "$tar\binary\$fn"
}

.\pre-process-list.ps1 $tar
