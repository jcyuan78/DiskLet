#include "pch.h"

LOCAL_LOGGER_ENABLE(L"simulator", LOGGER_LEVEL_DEBUGINFO);

#include "ssd-simulator.h"
#include "workload-analyzer.h"

//int CSSDSimulator::PopupEmptyBlock(FlashAddress& fadd, BlockInfo::BLOCK_TYPE type)
//{	// pop up a block from SLC cache
//	if (m_empty_list.size() == 0) THROW_ERROR(ERR_APP, L"there is no empty block.");
//	//		WORD block_id = m_empty_list[m_empty_num - 1];
//	WORD block_id = m_empty_list.front();
//	m_empty_list.pop_front();
//	//		if (block_id >= m_block_num || block_id == 0) THROW_ERROR(ERR_APP, L"invalid block id (%04X)", block_id);
//	//		m_empty_num--;
//			// 初始化block
//	BlockInfo& block = m_block_manager[block_id];
//	if (block.block_type != BlockInfo::EMPTY_BLK) THROW_ERROR(ERR_APP,
//		L"block is not empty, type=%d", block.block_type);
//	block.block_type = type;
//	if (type == BlockInfo::SLC_BLK) block.total_unit_num = m_page_num;
//	else if (type == BlockInfo::TLC_BLK) block.total_unit_num = m_page_num * 3;
//	else THROW_ERROR(ERR_APP, L"invalid block type (%d)", type);
//	block.valid_unit_num = 0;
//	block.last_unit = 0;
//
//	LOG_DEBUG(L"popup block=(%04X), type=%d, unit=%d, erase count=%d",
//		block_id, type, block.total_unit_num, block.block_erase_count);
//	fadd.block = block_id;
//	fadd.unit = 0;
//
//	return 1;
//}

void CSSDSimulator::Create(const std::wstring& config_fn)
{
	std::string str_fn;
	jcvos::UnicodeToUtf8(str_fn, config_fn);

	boost::property_tree::wptree root_pt;
	boost::property_tree::read_json(str_fn, root_pt);

	// read basic info
	UINT page_size = root_pt.get<UINT>(L"nand.page_size");
	UINT page_num = root_pt.get<UINT>(L"nand.page_num");
	UINT block_num = root_pt.get<UINT>(L"nand.block_num");
	UINT unit_num = root_pt.get<UINT>(L"logical_drive.unit_num");
	CreateDisk(page_size, page_num, block_num, unit_num);

	m_refresh_block_stripe = root_pt.get<UINT>(L"policy.refresh_block_stripe");
	m_guardband_block_stripe = root_pt.get<UINT>(L"policy.guardband_block_stripe");
	m_max_cache = root_pt.get<UINT>(L"policy.slc_cache_num");

	LoadFromProperty(root_pt);

	// read performance info
}


// 创建ssd，page_size: page大小，以4KB为单位，page_num以SLC block的page数
void CSSDSimulator::CreateDisk(UINT page_size, UINT page_num, UINT block_num, UINT unit_num)
{
	if (m_FTL || m_block_manager) THROW_ERROR(ERR_APP, L"device has already created");
	m_page_size = page_size;
	m_page_num = page_num;
	m_block_num = block_num;
	m_total_unit = unit_num;

	m_block_manager = new BlockInfo[block_num];
	memset(m_block_manager, 0, sizeof(BlockInfo) * block_num);

	m_FTL = new FlashAddress[unit_num];
	memset(m_FTL, 0xFF, sizeof(FlashAddress) * unit_num);

	UINT64 capacity = (UINT64)unit_num * 8;
	global.SetDiskInfo(capacity, 0);
}