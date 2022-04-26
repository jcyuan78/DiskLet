# set environment

$platform="x64"

if ($global:vs_ver -eq 2017) {
	$vs_dir = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2017\Community"
}
elseif ($global:vs_ver -eq 2019) {
	$vs_dir = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community"
}
else {write-error "Unknown vs version $($global:vs_ver)";}

$build_dir = "$vs_dir\VC\Auxiliary\Build\"
$tool_dir = "$vs_dir\Common7\Tools\"
$ide_dir = "$vs_dir\Common7\IDE\"

& $build_dir\vcvarsall.bat $platform

# insert projects to build
$projects = @();
#$projects += @{name="StorageManagementLib"; path="..\application\StorageManagementLib\"}
$projects += @{name="DiskLet"; path="..\application\DiskLet\"}
$projects += @{name="LibClone"; path="..\application\LibClone\"}
$projects += @{name="VolumeShadowTest"; path="..\application\VolumeShadowTest\"}

$configs = ("DEBUG_DYNAMIC", "RELEASE_DYNAMIC");


push-location
cd ..\build
foreach ($cc in $configs)
{
	write-host "Building for $cc" -Foreground "Yellow"
	foreach ($pp in $projects)
	{
		$pfile= [String]::format("{0}{1}.vcxproj", $pp.path, $pp.name)
		write-host "Building project $($pp.name)" -Foreground "Yellow"
		#foreach ($cc in $configs)
		#{
			& $ide_dir\devenv.com $pfile /build "$cc|$platform"
		#}
	}
}

pop-location

