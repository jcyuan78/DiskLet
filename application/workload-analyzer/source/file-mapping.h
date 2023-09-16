///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <jccmdlet-comm.h>

typedef int32_t FID;

class LbaSegment
{
public:

public:
	uint64_t start_lba;
	uint64_t length;
	FID fid;
	uint64_t offset;	//在文件内的偏移
};



class CLba2FileMapping
{
	// 根据LBA地址，查找对应的文件
public:
	CLba2FileMapping(void);
	~CLba2FileMapping(void) {};
public:
	void AddLbaSegment(uint64_t start_lba, uint64_t length, FID fid, uint64_t offset);
	bool FindFile(uint64_t lba, FID& fid, uint64_t& offset);
	void ClearMapping(void);
protected:
	// lba列表，必须按照LBA的顺序排序，不能有重叠的。查找时用二分法查找。
	std::vector<LbaSegment> m_lba_list;
	// 当前最大的lba，用于快速排序。
	uint64_t m_max_lba;
};




namespace file_mapping 
{
	[CmdletAttribute(VerbsData::Import, "FileMapping")]
	public ref class ImportFileMapping : public JcCmdLet::JcCmdletBase
	{
	public:
		ImportFileMapping(void) {};
		~ImportFileMapping(void) {};

	public:

	public:
		virtual void BeginProcessing()	override {};
		virtual void EndProcessing()	override {};
		virtual void InternalProcessRecord() override {};
	};


}