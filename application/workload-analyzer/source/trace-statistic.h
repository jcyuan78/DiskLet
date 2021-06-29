#pragma once


// 通过heap实现一个地址的排序
class CAddressRank
{
public:
	CAddressRank(void) : m_heap_read(nullptr), m_heap_write(nullptr), m_heap_access(nullptr) {}
	~CAddressRank(void)
	{
		Clear();
	}

public:
	enum VALUE_TYPE {
		VALUE_READ_COUNT = 1, VALUE_WRITE_COUNT = 2, VALUE_ACCESS_COUNT = 3,
	};
	class AddressPair
	{
	public:
		UINT add;
		UINT read_count;
		UINT write_count;
		UINT access_count;
	};
public:
	void InitialRank(size_t size)
	{
		Clear();
		m_heap_size = size;
		m_heap_read = new AddressPair[m_heap_size];
		memset(m_heap_read, 0, sizeof(AddressPair) * m_heap_size);
		m_heap_write = new AddressPair[m_heap_size];
		memset(m_heap_write, 0, sizeof(AddressPair) * m_heap_size);
		m_heap_access = new AddressPair[m_heap_size];
		memset(m_heap_access, 0, sizeof(AddressPair) * m_heap_size);
		m_heap_size_2 = m_heap_size / 2;
	}
	void AddAddress(UINT address, UINT read_count, UINT write_count)
	{
		AddressPair data;
		data.add = address;
		data.read_count = read_count;
		data.write_count = write_count;
		data.access_count = read_count + write_count;

		AddToHeap<ReadCountCompare>(m_heap_read, data);
		AddToHeap<WriteCountCompare>(m_heap_write, data);
		AddToHeap<AccessCountCompare>(m_heap_access, data);

	}
	size_t GetSize(void) const { return m_heap_size; }
	AddressPair& GetAddressData(int vtype, size_t ii)
	{
		switch (vtype)
		{
		case VALUE_READ_COUNT: return m_heap_read[ii]; break;
		case VALUE_WRITE_COUNT: return m_heap_write[ii]; break;
		case VALUE_ACCESS_COUNT: return m_heap_access[ii]; break;
		}
		return AddressPair();
	}

public:
	static const GUID Guid() { return m_guid; }

protected:
	void Clear(void)
	{
		delete[] m_heap_read;		m_heap_read = nullptr;
		delete[] m_heap_write;		m_heap_write = nullptr;
		delete[] m_heap_access;		m_heap_access = nullptr;
	}

	struct ReadCountCompare {
		static bool Compare(const AddressPair& a, const AddressPair& b) {
			return (a.read_count <= b.read_count);
		}
	};

	struct WriteCountCompare {
		static bool Compare(const AddressPair& a, const AddressPair& b) {
			return (a.write_count <= b.write_count);
		}
	};

	struct AccessCountCompare {
		static bool Compare(const AddressPair& a, const AddressPair& b) {
			return (a.access_count <= b.access_count);
		}
	};

	template <class C>
	void AddToHeap(AddressPair* heap, const AddressPair& data)
	{
		int cur = 1;
		if (C::Compare(data, heap[cur])) return;
		//		heap[cur].add = address;
		//		heap[cur].val = value;
		heap[cur] = data;
		while (cur < m_heap_size_2)
		{
			int left = cur * 2;
			int right = left + 1;
			int min = right;
			if (C::Compare(heap[cur], heap[left]))
			{
				if (C::Compare(heap[cur], heap[right])) break;
				else min = right;	// 和右边交换
			}
			else
			{
				if (C::Compare(heap[left], heap[right]))	min = left; // 和左边交换
				else min = right;
			}

			heap[0] = heap[cur];
			heap[cur] = heap[min];
			heap[min] = heap[0];
			cur = min;
		}
	}
	static const GUID m_guid;

protected:


	VALUE_TYPE m_value_type;

	AddressPair* m_heap_read;
	AddressPair* m_heap_write;
	AddressPair* m_heap_access;

	size_t m_heap_size;		// heap size必须是2的整幂
	size_t m_heap_size_2;	// heap_size的一半
};


class ITraceStatistic : public IJCInterface
{
public:
	enum { CMD_WRITE = 1, CMD_READ = 2, };
public:
	virtual void Initialize(UINT) = 0;
	virtual PSObject^ InvokeTrace(int cmd, UINT cluster, UINT cluster_num) = 0;
};

class CTraceStatistic : public ITraceStatistic
{
public:
	CTraceStatistic(void) { m_cmd_id = 0; }
	~CTraceStatistic(void) {};
public:
	virtual void Initialize(UINT cluster);
	virtual PSObject^ InvokeTrace(int cmd, UINT cluster, UINT cluster_num)
	{
		m_read_irg++;
		m_write_irg++;
		m_access_irg++;
		if (m_cluster >= cluster && m_cluster < (cluster + cluster_num))
		{
			if (cmd == CMD_WRITE)
			{
				m_write_count++;
				m_write_irg = 0;
				m_access_irg = 0;
			}
			else if (cmd == CMD_READ)
			{
				m_read_count++;
				m_read_irg = 0;
				m_access_irg = 0;
			}
		}
		m_avg_read_irg += m_read_irg;
		m_avg_write_irg += m_write_irg;
		m_avg_access_irg += m_access_irg;

		m_cmd_id++;
		if (m_cmd_id >= 1000)
		{
			PSObject^ res = gcnew PSObject;
			AddPropertyMember<UINT>(res, L"read_count", m_read_count);
			AddPropertyMember<UINT>(res, L"write_count", m_write_count);
			AddPropertyMember<UINT>(res, L"access_count", m_read_count + m_write_count);
			AddPropertyMember<UINT>(res, L"read_irg", m_avg_read_irg);
			AddPropertyMember<UINT>(res, L"write_irg", m_avg_write_irg);
			AddPropertyMember<UINT>(res, L"access_irg", m_avg_access_irg);

			m_avg_read_irg = 0;
			m_avg_write_irg = 0;
			m_avg_access_irg = 0;

			m_read_count = 0;
			m_write_count = 0;
			m_cmd_id = 0;
			return res;
		}
		else return nullptr;
	}
protected:
	UINT m_read_count;
	UINT m_write_count;
	UINT m_cluster_num;

	UINT m_read_irg, m_write_irg, m_access_irg;
	UINT m_avg_read_irg, m_avg_write_irg, m_avg_access_irg;


	UINT m_cmd_id;
	UINT m_cluster;
};