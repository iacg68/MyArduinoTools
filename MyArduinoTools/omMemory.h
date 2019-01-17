#ifndef OM_MEMORY_H
#define OM_MEMORY_H

#include <stdlib.h>

namespace om {

// provides some lightweight stl replacements
extern int debug;

template<typename T>
void _element_destructor(T* ptr)
{
	delete ptr;
}

template<typename T>
void _array_destructor(T* ptr)
{
	delete[] ptr;
}

template<typename T>
void _allocs_destructor(T* ptr)
{
	if (ptr) free(ptr);
}

template<typename T, void(*D)(T*) = _element_destructor>
class unique_ptr
{
public:
	unique_ptr()
	{}

	unique_ptr(T *ptr)
		: m_ptr(ptr)
	{}

	unique_ptr(unique_ptr &&other)
	{
		if (this == &other)
			return;
		m_ptr = other.m_ptr;
		other.m_ptr = nullptr;
	}

	unique_ptr& operator=(unique_ptr &&other)
	{
		reset(other.m_ptr);
		return *this;
	}

	unique_ptr& operator=(T *ptr)
	{
		reset(ptr);
		return *this;	
	}

	operator bool() const
	{
		return m_ptr;
	}

    T& operator* () const
    {
        return *m_ptr;
    }

    T* operator-> () const
    {
        return m_ptr;
    }

	T& operator[](uint32_t i) const
	{
		return m_ptr[i];
	}

	unique_ptr move()
	{
		return unique_ptr(release());
	}

	T* release()
	{
		T* temp = m_ptr;
		m_ptr = nullptr;
		return temp;
	}

	void swap(unique_ptr &other)
	{
		T* temp = m_ptr;
		m_ptr = other.m_ptr;
		other.m_ptr = temp;
	}

	T* get() const
	{
		return m_ptr;
	}

	void reset(T* ptr = nullptr)
	{
		if (ptr == m_ptr)
			return;
		D(m_ptr);
		m_ptr = ptr;
	}

	~unique_ptr()
	{
		reset();
	}

	unique_ptr(const unique_ptr &other) = delete;
	unique_ptr& operator=(const unique_ptr &other) = delete;

private:
	T*	m_ptr{nullptr};
};

}

#endif