# This script is used to update vc project file for fitting vs 2019 or vs 2017
param($vs_ver, $win_sdk="10.0.17763.0")
$tar = resolve-path "$pwd\..\"
$global:vs_ver = $vs_ver

$applications = dir $tar\*.vcxproj -recurse;
#- | ?{$_.Attributes -eq "Directory"}
#$applications += dir ..\jcvos2 | ?{$_.Attributes -eq "Directory"}
foreach ($app in $applications)
{
#	$proj_file = dir "$($app.FullName)\*.vcxproj"	
	$proj_file = $app;
#	if ($proj_file -ne $null)
#	{
		write-host "convert $($proj_file.FullName) to vs $vs_ver"
		.\update-vsproject.ps1 -proj_fn $($proj_file.FullName) -vs_ver $vs_ver -win_sdk $win_sdk
#	}
}
