#pragma once





class CGeneralMaker
{
public:
	CGeneralMaker(void);
	~CGeneralMaker(void);

public:
	bool MakeCopyStrategy(std::vector<CopyStrategy> & strategy, IDiskInfo * src, IDiskInfo * dst, UINT64 disk_size = (-1));

};