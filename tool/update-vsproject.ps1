# This script is used to update vc project file for fitting vs 2019 or vs 2017
param($proj_fn, $vs_ver, $win_sdk=0)
#convert event.csv file, parse the detail field

$fn = resolve-path $proj_fn;

$proj = [xml](Get-Content $fn);
write-debug "$proj"
$project = $proj.Project;
write-debug "$project"

$properties = $project.PropertyGroup;

#.PlatformToolset
foreach ($pp in $properties)
{
	$platform = $pp.PlatformToolset;
	if ($platform -ne $null)
	{
		write-debug "platform = $platform";
		if ($vs_ver -eq "2017") {
			write-debug "set version to v141";
			$pp.PlatformToolset = "v141"; 
		}
		elseif ($vs_ver -eq "2019") { $pp.platformtoolset = "v142"; }
	}
}

$properties = $project.PropertyGroup | ?{$_.Label -eq "Globals"};
write-debug "prop = $properties"

foreach ($pp in $properties)
{
	if ($pp.WindowsTargetPlatformVersion -ne $null)
	{
		write-debug "sdk = $($pp.WindowsTargetPlatformVersion)";
		if ($vs_ver -eq "2017") {
			write-debug "set version to v141";
			$pp.WindowsTargetPlatformVersion = $win_sdk; 
		}
		elseif ($vs_ver -eq "2019") { $pp.WindowsTargetPlatformVersion = "10.0"; }
	}
}

write-debug "$($proj.project.propertygroup.platformtoolset)"
write-debug "$($proj.project.propertygroup.WindowsTargetPlatformVersion)"

$proj.Save($fn);
