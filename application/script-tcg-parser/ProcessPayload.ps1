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
    $data | Export-Binary "$tar\binary\$($file.Name).bin"
}
