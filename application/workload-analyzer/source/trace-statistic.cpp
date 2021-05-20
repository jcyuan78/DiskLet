#include "pch.h"

LOCAL_LOGGER_ENABLE(L"statistic", LOGGER_LEVEL_DEBUGINFO);


#include "trace-statistic.h"

#include "workload-analyzer.h"

#include <boost/cast.hpp>


void CTraceStatistic::Initialize(UINT cluster)
{
	m_cmd_id = 0;
	UINT64 secs = global.GetDiskCapacity();
	m_cluster_num = boost::numeric_cast<UINT>((secs - 1) / 8 + 1);
	LOG_DEBUG(L"start counting, cluster number = %d", m_cluster_num);

	//m_read_count = new UINT[m_cluster_num];
	//memset(m_read_count, 0, sizeof(UINT) * m_cluster_num);
	//m_write_count = new UINT[m_cluster_num];
	//memset(m_write_count, 0, sizeof(UINT) * m_cluster_num);
	m_read_count = 0;
	m_write_count = 0;

//	m_cluster = boost::numeric_cast<UINT>(lba / 8);
	m_cluster = cluster;

	m_read_irg = 0;
	m_write_irg = 0;
	m_access_irg = 0;
	m_avg_read_irg = 0;
	m_avg_write_irg = 0;
	m_avg_access_irg = 0;


	//		m_cmd_num = 0;


}