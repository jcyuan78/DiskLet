#pragma once

#include <queue>
#include <list>
#include <boost/property_tree/json_parser.hpp>
#include <boost/bind.hpp>


#define T_SLC_PAGE_PROGRAM	(10)
#define T_SLC_PAGE_READ		(1)
#define T_TLC_MSB_PAGE_PROGRAM	(10)
#define T_TLC_CSB_PAGE_PROGRAM	(20)
#define T_TLC_LSB_PAGE_PROGRAM	(30)
#define T_TLC_MSB_PAGE_READ		(1)
#define T_TLC_CSB_PAGE_READ		(2)
#define T_TLC_LSB_PAGE_READ		(2)
#define T_BLOCK_ERASE		(50)
#define T_UNMAPPING_READ	(1)

//#define REFRESH_BLOCK_STRIP		(10)
//#define GUARDBAND_BLOCK_STRIP	(6)

class GlobalInit;
extern GlobalInit global;


class FlashAddress
{
public:
	WORD block;
	WORD unit;
// 统计信息
	UINT read_count;
	UINT write_count;
};

class BlockInfo
{
public:
	enum BLOCK_TYPE	{
		EMPTY_BLK=0, SLC_BLK=1, MLC_BLK=2, TLC_BLK=3, QLC_BLK=4,};
	BLOCK_TYPE block_type;
	WORD valid_unit_num;		// 有效page数	
	WORD total_unit_num;		// 总page数，根据SLC, MLC, TLC变化
	//WORD last_unit;				// 第一个empty unit的指针。
	WORD block_id;
	UINT* logical_unit;

// 统计信息
	UINT block_read_count;
	UINT block_write_count;
	UINT block_erase_count;

public:
	bool IsEmpty(void) { return (block_type == EMPTY_BLK); }
	// block是否被写满
	bool IsFull(void) { return (logical_unit[total_unit_num - 1] != UINT(-1)); }
	
	int Erase(UINT page_num)
	{
		block_type = BlockInfo::EMPTY_BLK;
		valid_unit_num = 0;
		total_unit_num = 0;
		//last_unit = 0;
		memset(logical_unit, 0xFF, sizeof(UINT) * page_num * 3);
		block_erase_count++;
		return T_BLOCK_ERASE;
	}

	int Program(WORD & next_unit, UINT src_unit)
	{
		if (next_unit >= total_unit_num)
		{
			THROW_ERROR(ERR_APP, L"invalid unit, ptr=%08X, total unit=%08X", next_unit, total_unit_num);
		}
		if (logical_unit[next_unit] != (UINT)-1)
		{
			THROW_ERROR(ERR_APP, L"unit is not empty, fadd(%04X:%04X), logical unit=%08X",
				block_id, next_unit, logical_unit[next_unit]);
		}
		logical_unit[next_unit] = src_unit;
		valid_unit_num++;
		next_unit++;
		block_write_count++;
		if (next_unit >= total_unit_num) next_unit = 0xFFFF;
		//计算消耗时间
		return 0;
	}
};

class HotIndex
{
public:
	HotIndex(void): m_age(nullptr) {};
	~HotIndex(void) { delete m_age; }
public:
	void Create(UINT unit_num)
	{
		delete m_age;
		m_age = new UINT[unit_num];
		memset(m_age, 0, sizeof(UINT) * unit_num);
		m_timestamp = 0;
	}
	void Touch(UINT unit)
	{
		m_age[unit] = m_timestamp;
		m_timestamp++;
	}
	UINT GetAge(UINT unit) { return m_timestamp - m_age[unit]; }
protected:
	UINT* m_age;
	UINT m_timestamp;
};

typedef BlockInfo * LP_BLK_INFO;

class CSSDSimulator
{
public:
	CSSDSimulator(void) : m_FTL(nullptr), m_block_manager(nullptr)
	{

	}
	virtual ~CSSDSimulator(void)
	{
		delete [] m_FTL;
		for (UINT bb = 0; bb < m_block_num; bb++)
		{
			delete[] (m_block_manager[bb].logical_unit);
		}
		delete [] m_block_manager;;
	}

public:
	void Create(const std::wstring& config_fn);

	virtual void LoadFromProperty(const boost::property_tree::wptree& prop) { }
	virtual void EnableHotIndex(int enable) {};


	// 创建ssd，page_size: page大小，以4KB为单位，page_num以SLC block的page数
	void CreateDisk(UINT page_size, UINT page_num, UINT block_num, UINT unit_num);

	// 初始化ssd，清除FTL所有table
	virtual void Initialize(UINT cache_num)
	{
		memset(m_block_manager, 0, sizeof(BlockInfo)*m_block_num);
		memset(m_FTL, 0xFF, sizeof(FlashAddress)*m_total_unit);
		for (UINT bb=1; bb<(m_block_num-1); bb++)
		{
			m_block_manager[bb].block_id = bb;
			m_empty_list.push_back(bb);
		}

		m_data_blocks.clear();
		m_cache.clear();

		if (cache_num != 0) m_max_cache = cache_num;		
		m_write_ptr.block = 0xFFFF, m_write_ptr.unit = 0xFFFF;
		m_tlc_active.block = 0xFFFF, m_tlc_active.unit = 0xFFFF;
		m_valid_logical_unit = 0;
		ResetStatistic();
	}

	void ResetStatistic(void)
	{
		m_SLC_read = 0;
		m_TLC_read = 0;
		m_unmapping_read = 0;
		m_host_write = 0;
		m_nand_write = 0;
	}

	void InvalidUnit(FlashAddress& fadd)
	{
		BlockInfo& cur_blk = m_block_manager[fadd.block];
		if (cur_blk.block_type == BlockInfo::EMPTY_BLK)
		{
			THROW_ERROR(ERR_APP, L"cur block %04X is empty block", fadd.block);
		}
		if (cur_blk.valid_unit_num == 0)
		{
			THROW_ERROR(ERR_APP, L"there is no valid unit in block %04X", fadd.block);
		}
		cur_blk.valid_unit_num--;
		// debug only
		if (cur_blk.logical_unit[fadd.unit] & 0xF0000000)
		{
			THROW_ERROR(ERR_APP, L"unit has already invalid=%08X", cur_blk.logical_unit[fadd.unit]);
		}

		cur_blk.logical_unit[fadd.unit] |= 0xF0000000;
	}

	// 读写函数, 返回执行所需时间，负数为错误代码。
	virtual int WriteSecs(UINT lba, UINT secs)
	{
		// convert lba to unit
		UINT unit, unit_num;
		Lba2Unit(lba, secs, unit, unit_num);
		if (unit >= m_total_unit || (unit + unit_num) > m_total_unit) THROW_ERROR(ERR_APP, L"logical address over range, lba=0x%08X, secs=%d", lba, secs);
		int local_time_cost = 0;

		for (UINT uu = 0; uu < unit_num; uu++, unit++)
		{
			if (m_write_ptr.block == 0xFFFF)
			{	// pop up new block
				local_time_cost = PopupEmptyBlock(m_write_ptr, BlockInfo::SLC_BLK);
			}
			// 写入数据，saniti check
			BlockInfo& active_block = m_block_manager[m_write_ptr.block];
			if (active_block.block_type == BlockInfo::EMPTY_BLK) THROW_ERROR(ERR_APP, L"active block %04X is empty", m_write_ptr.block);
			// invalid current unit
			FlashAddress& cur_f_add = m_FTL[unit];
			if (cur_f_add.block != 0xFFFF)
			{	// 有效地址
				InvalidUnit(cur_f_add);
			}
			else
			{	// logic unit从无效变成有效，增加有效逻辑unit数量
				m_valid_logical_unit++;
			}
			int ir = ProgramNandPage(m_write_ptr, unit);
			if (ir < 0) THROW_ERROR(ERR_APP, L"error in program nand , res=%d", ir);
			local_time_cost += ir;
			local_time_cost += CheckForgroundGC();

			TouchUnit(unit);

			m_FTL[unit].write_count++;
			m_host_write++;
		}
		// 显示统计信息
		LOG_DEBUG_(1, L"logical saturation=%.2f%%, data=%d, cache=%d, empty=%d", 
			(float)m_valid_logical_unit / m_total_unit * 100, m_data_blocks.size(), m_cache.size(), m_empty_list.size());

		// background不计入执行时间
		int cost = CheckBackgroundGC();

		return local_time_cost;
	}

	virtual int ReadSecs(UINT lba, UINT secs)
	{
		UINT unit, unit_num;
		Lba2Unit(lba, secs, unit, unit_num);
		if (unit >= m_total_unit || (unit + unit_num) > m_total_unit) THROW_ERROR(ERR_APP, L"logical address over range, lba=0x%08X, secs=%d", lba, secs);
		int local_time_cost = 0;
		for (UINT uu = 0; uu < unit_num; uu++, unit++)
		{
			FlashAddress& fadd = m_FTL[unit];
			fadd.read_count++;
			// Unmapping read
			if (fadd.block == 0xFFFF)
			{
				local_time_cost += T_UNMAPPING_READ;
				m_unmapping_read++;
				continue;
			}
			BlockInfo& block = m_block_manager[fadd.block];
			if (block.block_type == BlockInfo::EMPTY_BLK) THROW_ERROR(ERR_APP, L"read data in an empty block, unit=%08X, block=%04X", unit, fadd.block);
			if (block.logical_unit[fadd.unit] != unit)
			{
				THROW_ERROR(ERR_APP, L"logical address unmatch, fadd=(%04X:%04X), f2h=%08X, h2f=%08X",
					fadd.block, fadd.unit, block.logical_unit[fadd.unit], unit);
			}
			if (block.block_type == BlockInfo::SLC_BLK)
			{
				local_time_cost += T_SLC_PAGE_READ;
				m_SLC_read++;
			}
			else if (block.block_type == BlockInfo::TLC_BLK)
			{
				UINT page_type = fadd.unit % 3;
				switch (page_type)
				{
				case 0: local_time_cost += T_TLC_MSB_PAGE_READ; break;
				case 1: local_time_cost += T_TLC_CSB_PAGE_READ; break;
				case 2: local_time_cost += T_TLC_CSB_PAGE_READ; break;
				}
				m_TLC_read++;
			}
			// 统计信息
			TouchUnit(unit);
			block.block_read_count++;
		}
		LOG_DEBUG_(1, L"unmapping read=%d, SLC read=%d, TLC read=%d", m_unmapping_read, m_SLC_read, m_TLC_read);
		return local_time_cost;
	}

	float GetLogicalSaturation(void) {	return ((float)m_valid_logical_unit / (float)m_total_unit);	}

	UINT GetDataBlockNum(void) { return m_data_blocks.size(); }
	UINT GetSLCBlockNum(void) { return m_cache.size(); }
	UINT GetEmptyBlockNum(void) { return m_empty_list.size(); }


protected:
	int PopupEmptyBlock(FlashAddress& fadd, BlockInfo::BLOCK_TYPE type)
	{	// pop up a block from SLC cache
		if (m_empty_list.size() == 0) THROW_ERROR(ERR_APP, L"there is no empty block.");
		WORD block_id = m_empty_list.front();
		m_empty_list.pop_front();
		// 初始化block
		BlockInfo& block = m_block_manager[block_id];
		if (block.logical_unit == nullptr)block.logical_unit = new UINT[m_page_num * 3];	// align to TLC;
		memset(block.logical_unit, 0xFF, sizeof(UINT) * m_page_num * 3);
		
		if (block.block_type != BlockInfo::EMPTY_BLK)
		{
			THROW_ERROR(ERR_APP, L"block is not empty, type=%d", block.block_type);
		}
		block.block_type = type;
		if (type == BlockInfo::SLC_BLK) block.total_unit_num = m_page_num;
		else if (type == BlockInfo::TLC_BLK) block.total_unit_num = m_page_num * 3;
		else THROW_ERROR(ERR_APP, L"invalid block type (%d)", type);
		block.valid_unit_num = 0;
		// Block放入相应的队列中
		if (type == BlockInfo::SLC_BLK) m_cache.push_back(block_id);
		else	m_data_blocks.push_back(block_id);
		
		LOG_DEBUG(L"popup block=(%04X), type=%d, unit=%d, erase count=%d",
			block_id, type, block.total_unit_num, block.block_erase_count);
		fadd.block = block_id;
		fadd.unit = 0;
	
		return 1;
	}

	int ProgramNandPage(FlashAddress & dst_ptr, UINT src_unit)
	{
		int local_time_cost = 0;
		BlockInfo& dst_blk = m_block_manager[dst_ptr.block];
		// 写入数据，saniti check
		WORD funit = dst_ptr.unit;
		local_time_cost += dst_blk.Program(funit, src_unit);
		// 更新FTL
		FlashAddress& cur_f_add = m_FTL[src_unit];
		cur_f_add.block = dst_ptr.block;
		cur_f_add.unit = dst_ptr.unit;
		// block已满
		if (funit == 0xFFFF) dst_ptr.block = 0xFFFF, dst_ptr.unit = 0xFFFF;
		else dst_ptr.unit = funit;
		// 更新统计信息：
		cur_f_add.write_count++;
		m_nand_write++;
		return local_time_cost;
	}

	int EraseBlock(BlockInfo& blk)
	{
		int local_time_cost = blk.Erase(m_page_num);
		m_empty_list.push_back(blk.block_id);
		return local_time_cost;
	}

	int FoldSLC(void)
	{	// 从SLC移到TLC，填满一个TLC
		int local_time_cost = 0;
		
		// popup a TLC block
		FlashAddress fdst;
		int ir = PopupEmptyBlock(fdst, BlockInfo::TLC_BLK);
		if (ir < 0) THROW_ERROR(ERR_APP, L"failed on popup empty block, res=%d", ir);
		local_time_cost += ir;
		JCASSERT(fdst.block < m_block_num);

		while (1)
		{	// pickup one SLC block
			WORD src_blk_id = m_cache.front();
			BlockInfo& src_blk = m_block_manager[src_blk_id];
			// 如果block不满，则不做GC
			if (!src_blk.IsFull()) return local_time_cost;
			// copy all valid pages to TLC
			for (int pp = 0; pp < src_blk.total_unit_num; pp++)
			{
				UINT unit = src_blk.logical_unit[pp];
				if (unit == (UINT)-1) THROW_ERROR(ERR_APP, L"invalid empty unit, fadd=(%04X:%04X)", src_blk_id, pp);
				FlashAddress& fadd = m_FTL[unit];
				if (fadd.block == src_blk_id && fadd.unit == pp)
				{	// 移动
					local_time_cost += T_SLC_PAGE_READ;
					src_blk.block_read_count++;
					InvalidUnit(fadd);
					ir = ProgramNandPage(fdst, unit);
					if (ir < 0) THROW_ERROR(ERR_APP, L"failed on programming nand res=%d", ir);
					local_time_cost += ir;

					if (fdst.block == 0xFFFF || fdst.unit == 0xFFFF) break;
				}
			}
			if (src_blk.valid_unit_num != 0)
			{
				THROW_ERROR(ERR_APP, L"sorce block valid page unmatched, blk=(%04X:0000), valid unit=%d", src_blk_id, src_blk.valid_unit_num);
			}
			// erase source block
			m_cache.pop_front();
			m_empty_list.push_back(src_blk_id);
			ir = src_blk.Erase(m_page_num);
			if (ir < 0) THROW_ERROR(ERR_APP, L"failed on erasing block (%04X)", src_blk_id);
			local_time_cost += ir;

			if (fdst.block == 0xFFFF || fdst.unit == 0xFFFF) break;
		}
		return local_time_cost;
	}

	int FoldTLC(void)
	{
		return 0;
	}

	static bool IsEqual(WORD block_id, const WORD& a)
	{
		return (block_id == a);
	}

	virtual FlashAddress& GetFoldingDest(UINT unit, int local_time_cost)
	{
		if (m_tlc_active.block == 0xFFFF || m_tlc_active.unit == 0xFFFF)
		{
			local_time_cost = PopupEmptyBlock(m_tlc_active, BlockInfo::TLC_BLK);
		}
		return m_tlc_active;
	}

	virtual void TouchUnit(UINT unit) {};


	// 挑选valid page最少的data block，clean到当前active block
	//	active [in/out]: 当前active block。Folding 会改变active block的指针。
	//	dst_type [in]: active block的类型，SLC还是TLC，重新pop empty block的时候会用到。
	virtual int Folding(/*FlashAddress & active, BlockInfo::BLOCK_TYPE dst_type*/)
	{
		int local_time_cost = 0;
		// 挑选Valid Page 最少的data block
		WORD min_valid = m_page_num * 3;
		WORD min_block = 0;
		for (WORD bb = 1; bb < m_block_num; ++bb)
		{
			BlockInfo& block = m_block_manager[bb];
			if (block.IsEmpty() || (!block.IsFull()) ) continue;
			WORD valid_unit = block.valid_unit_num;
			if (block.block_type == BlockInfo::SLC_BLK) valid_unit *= 3;

//			if ( (!block.IsEmpty()) && (block.IsFull()) && (block.valid_unit_num < min_valid))
//			{
			if (valid_unit < min_valid)
			{
				min_block = bb;
				min_valid = valid_unit;
			}
		}
		if (min_block == 0)
		{
			LOG_DEBUG(L"slc cache=%d, tlc_cache=%d", m_cache.size(), m_data_blocks.size());
			THROW_ERROR(ERR_APP, L"cannot find block with mim valid page, mim_block=0, min_valid=%d", min_valid);
		}
		BlockInfo& src_blk = m_block_manager[min_block];
		LOG_DEBUG(L"fold from (%04X:0000), valid unit=%d", min_block, src_blk.valid_unit_num);
		// 将这个block的数据所有valid page移动到active block
		for (int pp = 0; pp < src_blk.total_unit_num; pp++)
		{
			UINT unit = src_blk.logical_unit[pp];
			// for debug: 这个unit已经无效
			if (unit & 0xF0000000) continue;
			if (unit == (UINT)-1) THROW_ERROR(ERR_APP, L"invalid empty unit, fadd=(%04X:%04X)", min_block, pp);
			FlashAddress& fadd = m_FTL[unit];
			// 比对FTL，判断是否是valid unit
			if (fadd.block == min_block && fadd.unit == pp)
			{	// 移动
				if (src_blk.valid_unit_num == 0) THROW_ERROR(ERR_APP, L"valid unit of source block =0, fadd=(%04X:%04X)", min_block, pp);
				local_time_cost += T_SLC_PAGE_READ;
				InvalidUnit(fadd);
				src_blk.block_read_count++;
				// check destination
				FlashAddress& fdst = GetFoldingDest(unit, local_time_cost);
				//if (active.block == 0xFFFF || active.unit == 0xFFFF)
				//{
				//	local_time_cost += PopupEmptyBlock(active, dst_type);
				//}
				// write to destination
				int ir = ProgramNandPage(fdst, unit);
				if (ir < 0) THROW_ERROR(ERR_APP, L"failed on programming nand res=%d", ir);
				local_time_cost += ir;
			}
		}
		if (src_blk.valid_unit_num != 0)
		{
			THROW_ERROR(ERR_APP, L"source block valid page unmatched, blk=(%04X:0000), valid unit=%d", 
				min_block, src_blk.valid_unit_num);
		}
		// erase block
		// 从队列中移除
		if (src_blk.block_type == BlockInfo::SLC_BLK)
		{
			LOG_DEBUG(L"remove blk:(%04X:0000) from slc cache, size before=%d", min_block, m_cache.size());
			m_cache.remove_if(boost::bind(IsEqual, min_block, _1));
			LOG_DEBUG(L"after size=%d", m_cache.size());
		}
		else if (src_blk.block_type == BlockInfo::TLC_BLK)
		{
			m_data_blocks.remove_if(boost::bind(IsEqual, min_block, _1));
		}
		else THROW_ERROR(ERR_APP, L"unknow block type (%d) of folding source:(%04X:0000)", src_blk.block_type, min_block);
		EraseBlock(src_blk);
	}

	int CheckForgroundGC(void)
	{
		int local_time_cost = 0;
		float logical_saturation = GetLogicalSaturation();
		UINT forground_fold_stop = m_refresh_block_stripe + m_guardband_block_stripe;

		if (m_empty_list.size() < m_refresh_block_stripe)
		{	// start background gc
			while (m_empty_list.size() < forground_fold_stop)
			{
				LOG_DEBUG(L"start foreground gc, empty=%d, ls=%.2f, start=%d, stop=%d", 
					m_empty_list.size(), logical_saturation, m_refresh_block_stripe, forground_fold_stop);
				local_time_cost += Folding(/*m_tlc_active, BlockInfo::TLC_BLK*/);
			}
		}
		return local_time_cost;		return 0;
	}

	int CheckBackgroundGC(void)
	{
		int local_time_cost = 0;
		float logical_saturation = GetLogicalSaturation();
		m_background_fold_start = 0.2 * m_block_num * (1 - logical_saturation);
		m_background_fold_stop = 0.6 * m_block_num * (1 - logical_saturation) 
			+ (m_refresh_block_stripe + m_guardband_block_stripe) * logical_saturation;

		//LOG_DEBUG_(1, L"check background gc, empty=%d, ls=%.2f, start=%d, stop=%d", 
		//	m_empty_list.size(), logical_saturation, m_background_fold_start, m_background_fold_stop);
		if (m_empty_list.size() < m_background_fold_start)
		{	// start background gc
			while (m_empty_list.size() < m_background_fold_stop)
			{
				LOG_DEBUG(L"start background gc, empty=%d, ls=%.2f, start=%d, stop=%d", 
					m_empty_list.size(), logical_saturation, m_background_fold_start, m_background_fold_stop);
				local_time_cost += Folding(/*m_tlc_active, BlockInfo::TLC_BLK*/);
			}
		}
		return local_time_cost;
	}

	void Lba2Unit(UINT lba, UINT secs, UINT& unit, UINT& unit_num)
	{
		unit = lba / 8;
		UINT end_lba = lba+secs;
		if (end_lba == 0) THROW_ERROR(ERR_APP, L"invalid logical address, lba=%d, secs=%d", lba, secs);
		UINT end_unit = (end_lba - 1) / 8 + 1;
		unit_num = end_unit - unit;
	}

	// 获取统计信息

protected:
	UINT m_page_size;	// page大小， unit单位
	UINT m_page_num;	// 每个block的page数量，以SLC为准。当pop empty block时，填入total_unit_num;
	UINT m_block_num;	// block 数量
	UINT m_total_unit;	// logical unit数量
	UINT m_max_cache;

	// 有效逻辑unit数量，用于计算逻辑饱和度
	UINT m_valid_logical_unit;
	UINT m_refresh_block_stripe;
	UINT m_guardband_block_stripe;
	UINT m_background_fold_start;
	UINT m_background_fold_stop;


	FlashAddress *m_FTL;
	BlockInfo * m_block_manager;
	std::list<WORD> m_empty_list;
	std::list<WORD> m_cache;
	std::list<WORD> m_data_blocks;
	FlashAddress m_write_ptr;
	FlashAddress m_tlc_active;

// 统计信息：
public:
	UINT64 m_SLC_read;
	UINT64 m_TLC_read;
	UINT64 m_unmapping_read;
	UINT64 m_host_write;
	UINT64 m_nand_write;
};

class CSSDSimulator_2 : public CSSDSimulator
{
public:
	CSSDSimulator_2(void) {};
	virtual ~CSSDSimulator_2(void) {};

public:
	virtual FlashAddress& GetFoldingDest(UINT unit, int local_time_cost)
	{
		// age越小，数据越是hot
		UINT age = m_hot_index.GetAge(unit);
//		if (m_runtime_thre> 0) LOG_DEBUG(L"fold unit=%08X, age=%d, threshold=%d", unit, age, m_runtime_thre)
		FlashAddress& active = (age < m_runtime_thre) ? m_write_ptr : m_tlc_active;
		if (active.block == 0xFFFF || active.unit == 0xFFFF)
		{
			BlockInfo::BLOCK_TYPE type = (age < m_runtime_thre) ? BlockInfo::SLC_BLK : BlockInfo::TLC_BLK;
			local_time_cost = PopupEmptyBlock(active, type);
		}
		return active;
	}

	virtual void TouchUnit(UINT unit) {
		m_hot_index.Touch(unit);
	}

	virtual void Initialize(UINT cache_num)
	{
		__super::Initialize(cache_num);
		m_hot_index.Create(m_total_unit);
	}

	virtual void LoadFromProperty(const boost::property_tree::wptree& prop)
	{
		m_hot_threshold = prop.get<UINT>(L"policy.hot_threshold");
	}

	virtual void EnableHotIndex(int enable)
	{
		if (enable) m_runtime_thre = m_hot_threshold;
		else m_runtime_thre = 0;
	}


protected:
	UINT m_hot_threshold;
	UINT m_runtime_thre;
	HotIndex m_hot_index;

};