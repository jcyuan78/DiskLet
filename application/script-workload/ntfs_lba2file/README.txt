
1, Start PowerShell as Admin
2, move to the tool dir
3, run command
	.\ntfs_start.ps1 -drive <drive> -offset <start_LBA> -out_fn <output_file>
   <drive> is the drive letter, like "C:", "D:"
   <start_LBA> is the start LBA of partition. If not specifi the offset, is use partition related logical address.
   <output_file> a text or csv file name to output data.