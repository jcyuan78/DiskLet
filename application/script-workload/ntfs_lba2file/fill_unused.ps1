param($offset)

begin
{
	[int64]$next_start = $offset
}
process
{
	if ($next_start -lt $_.start)
	{
		$len = $_.start - $next_start
		$lba_map = new-object object
		add-member -name start -value $next_start -inputobject $lba_map -membertype noteproperty
		add-member -name len -value $len -inputobject $lba_map   -membertype noteproperty
		add-member -name fid -value -1 -inputobject $lba_map  -membertype noteproperty
		add-member -name attr -value "" -inputobject $lba_map    -membertype noteproperty
		add-member -name fn -value "=UNUSED=" -inputobject $lba_map -membertype noteproperty
		$lba_map
	}
	$_
	$next_start = $_.start + $_.len
}
end
{
}