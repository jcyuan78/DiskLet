#include "pch.h"

#include "file-mapping.h"

CLba2FileMapping::CLba2FileMapping(void)
	: m_max_lba(0)
{
}

void CLba2FileMapping::AddLbaSegment(uint64_t start_lba, uint64_t length, FID fid, uint64_t offset)
{
	// 目前假定输入是按照LBA排序的
	if (start_lba <= m_max_lba) THROW_ERROR(ERR_APP, L"insert data is not in-order, max lba=%08llX, inserted=%08llX",
		m_max_lba, start_lba);
	LbaSegment seg;
	seg.start_lba = start_lba;
	seg.length = length;
	seg.fid = fid;
	seg.offset = offset;
	m_lba_list.push_back(seg);
	m_max_lba = start_lba + length;
}

bool CLba2FileMapping::FindFile(uint64_t lba, FID& fid, uint64_t& offset)
{
	size_t head = 0, tail = m_lba_list.size();
	while (head < tail)
	{
		size_t cur = (head + tail) / 2;
		LbaSegment& seg = m_lba_list[cur];
		if (lba < seg.start_lba) {
			tail = cur; 
		}
		else if (lba >= (seg.start_lba + seg.length)) {
			head = cur; }
//		if ((seg.start_lba <= lba) && (lba < seg.start_lba + seg.length))
		else {	// 找到匹配的段
			fid = seg.fid;
			offset = seg.offset;
			return true;
		}
	}
	fid = -1;
	offset = 0;
	return false;
}

void CLba2FileMapping::ClearMapping(void)
{
	m_lba_list.clear();
	m_max_lba = 0;
}
