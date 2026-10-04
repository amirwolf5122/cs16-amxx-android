// LP64-safe opaque handle tables for hamsandwich.
//
// AMX cells are 32-bit wide even on 64-bit builds, so raw heap pointers
// cannot round-trip through a cell without truncation. These small tables
// hand out sequential 32-bit ids instead (0 stays "invalid").

#ifndef HAM_HANDLES_H
#define HAM_HANDLES_H

#include <stddef.h>
#include <stdint.h>

#include <amtl/am-vector.h>

template <typename T>
class HamHandleTable
{
public:
	int32_t create(T *ptr)
	{
		m_ptrs.append(ptr);
		// ids start at 1 so 0 remains the "invalid handle" marker
		return (int32_t)m_ptrs.length();
	}

	T *lookup(int32_t handle) const
	{
		if (handle <= 0 || (size_t)handle > m_ptrs.length())
			return nullptr;

		return m_ptrs[handle - 1];
	}

private:
	ke::Vector<T *> m_ptrs;
};

#endif // HAM_HANDLES_H
