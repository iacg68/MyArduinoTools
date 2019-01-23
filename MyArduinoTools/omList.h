#ifndef OM_LIST_H
#define OM_LIST_H

#include <stddef.h>

namespace om {

template<typename T>
class list
{
private:
	struct NodeBase
	{
		NodeBase*	m_prev {nullptr};
		NodeBase*	m_next {nullptr};

		NodeBase()
			: m_prev(this)
			, m_next(this)
		{ }

		NodeBase(const NodeBase& other)
			: m_prev(other.m_prev)
			, m_next(other.m_next)
		{ }

		NodeBase(NodeBase &&other)
			: m_prev(other.m_prev)
			, m_next(other.m_next)
		{ other.clear(); }

		NodeBase move()
		{
			NodeBase copy(*this);
			clear();
			return copy;
		}

		void clear()
		{
			m_prev = this;
			m_next = this;
		}

		void extract()
		{
			m_prev->m_next = m_next;
			m_next->m_prev = m_prev;
		}

		void insert(NodeBase* at)
		{
			m_next = at;
			m_prev = at->m_prev;

			m_next->m_prev = this;
			m_prev->m_next = this;
		}
	};

	struct Node : NodeBase
	{
		T m_data;

		Node(const T &e)
			: NodeBase()
			, m_data(e)
		{}

		Node(T &&e)
			: NodeBase()
			, m_data(e)
		{}
	};

public:
	class iterator
	{
	public:
		iterator()
		{}

		T &operator*() const
		{
			return reinterpret_cast<Node*>(m_node)->m_data;
		}

		T *operator->() const
		{
			return &(reinterpret_cast<Node*>(m_node)->m_data);
		}

		bool operator != (const iterator &rhs) const
		{
			return m_node != rhs.m_node;
		}

		bool operator == (const iterator &rhs) const
		{
			return m_node == rhs.m_node;
		}

		iterator& operator++(int)
		{
			m_node = m_node->m_next;
			return *this;
		}

		iterator operator++()
		{
			iterator before(*this);
			m_node = m_node->m_next;
			return before;
		}

		iterator& operator--(int)
		{
			m_node = m_node->m_prev;
			return *this;
		}

		iterator operator--()
		{
			iterator before(*this);
			m_node = m_node->m_prev;
			return before;
		}

		iterator& operator += (size_t count)
		{
			while (count-- > 0)
				m_node = m_node->m_next;
			return *this;
		}

		iterator& operator -= (size_t count)
		{
			while (count-- > 0)
				m_node = m_node->m_prev;
			return *this;
		}

		iterator operator+(size_t count) const
		{
			iterator copy(*this);
			return copy += count;
		}

		iterator operator-(size_t count) const
		{
			iterator copy(*this);
			return copy -= count;
		}

		size_t operator-(const iterator &rhs) const
		{
			size_t count{0};
			for(const NodeBase* scan = rhs.m_node; scan != m_node; scan = scan->m_next)
				++count;
			return count;
		}

	private:
		friend class list<T>;
		iterator(NodeBase *ptr)
			: m_node(ptr)
		{}

		NodeBase *m_node {nullptr};
	};

	// ========================================================================

	list()
	{}

	list(const list<T> &other)
	{
		insert(end(), other.begin(), other.end());
	}

	list(list<T> &&other)
		: m_size(other.m_size)
		, m_end(other.m_end.move())
	{
		other.m_size  = 0;
	}

	size_t size() const
	{
		return m_size;
	}

	size_t memSize() const
	{
		return sizeof(list<T>) + size() * sizeof(Node);
	}

	iterator begin() const
	{
		return iterator(m_end.m_next);
	}

	iterator end() const
	{
		// as special value for end() we need its address only.
		// However it's illegal to refer to end() as real node
		return iterator(const_cast<NodeBase*>(&m_end));
	}

	void resize(size_t count)
	{
		while(m_size > count)
			delete unlink(m_end.m_prev);
		while(m_size < count)
			insertElement(T{}, &m_end);
	}

	void resize(size_t count, const T& value)
	{
		while(m_size > count)
			delete unlink(m_end.m_prev);
		while(m_size < count)
			insertElement(value, &m_end);
	}

	bool empty() const
	{
		return m_size == 0;
	}

	T& front()
	{
		return reinterpret_cast<Node*>(m_end.m_next)->m_data;
	}

	T& back()
	{
		return reinterpret_cast<Node*>(m_end.m_prev)->m_data;
	}

	void push_front(const T &e)
	{
		insertElement(e, m_end.m_next);
	}

	void push_front(T &&e)
	{
		insertElement(e, m_end.m_next);
	}

	void push_back(const T &e)
	{
		insertElement(e, &m_end);
	}

	void push_back(T &&e)
	{
		insertElement(e, &m_end);
	}

	iterator insert(iterator pos, const T &e)
	{
		return iterator(insertElement(e, pos.m_node));
	}

	void insert(iterator pos, size_t count, const T &e)
	{
		while (count-- > 0)
			insertElement(e, pos.m_node);
	}

	void insert(iterator pos, iterator first, iterator last)
	{
		for(NodeBase* scan = first.m_node; scan != last.m_node; scan = scan->m_next)
			insertElement(reinterpret_cast<Node*>(scan)->m_data, pos.m_node);
	}

	void splice(iterator pos, list<T> &other)
	{
		while(other.m_size > 0)
			insertNode(other.unlink(other.m_end.m_next), pos.m_node);
	}

	void splice(iterator pos, list<T> &other, iterator it)
	{
		insertNode(other.unlink(it.m_node), pos.m_node);
	}

	void splice(iterator pos, list<T> &other, iterator first, iterator last)
	{
		while(first != last)
		{
			NodeBase* node = other.unlink(first.m_node);
			++first; // still works, since node not deleted
			insertNode(node, pos.m_node);
		}
	}

	void sort()
	{
		for (bool sorted = false; !sorted; )
		{
			sorted = true;	// give it a try
			NodeBase* stopAtMax = &m_end;
			for (NodeBase* scan = m_end.m_next; scan != stopAtMax; )
			{
				NodeBase* next = scan->m_next;
				if (   (next != &m_end)
					&& (  reinterpret_cast<Node*>(next)->m_data
						< reinterpret_cast<Node*>(scan)->m_data))
				{
					// swap and move maximum to end
					insertNode(unlink(next), scan);
					sorted = false;		// ... need a verification run
				}
				else if (next == stopAtMax)
					stopAtMax = scan;	// ... and exit loop
				else
					scan = next;
			}
		}
	}

	template<typename F>
	void sort( F lessThan )
	{
		for (bool sorted = false; !sorted; )
		{
			sorted = true;	// give it a try
			NodeBase* stopAtMax = &m_end;
			for (NodeBase* scan = m_end.m_next; scan != stopAtMax; )
			{
				NodeBase* next = scan->m_next;
				if (    (next != &m_end)
				     && lessThan( reinterpret_cast<Node*>(next)->m_data
								, reinterpret_cast<Node*>(scan)->m_data))
				{
					// swap and move maximum to end
					insertNode(unlink(next), scan);
					sorted = false;		// ... need a verification run
				}
				else if (next == stopAtMax)
					stopAtMax = scan;	// ... and exit loop
				else
					scan = next;
			}
		}
	}

	void pop_front()
	{
		delete unlink(m_end.m_next);
	}

	void pop_back()
	{
		delete unlink(m_end.m_prev);
	}

	void erase(iterator pos)
	{
		delete unlink(pos.m_node);
	}

	void erase(iterator first, iterator last)
	{
		while (first != last)
		{
			NodeBase* toRemove = first.m_node;
			++first;
			delete unlink(toRemove);
		}
	}

	void remove(const T& data)
	{
		NodeBase* scan = m_end.m_next;
		while(scan != &m_end)
		{
			if (reinterpret_cast<Node const*>(scan)->m_data == data)
				scan = eraseAndNext(scan);
			else
				scan = scan->m_next;
		}
	}

	template <typename F>
	void remove_if( F test )
	{
		NodeBase* scan = m_end.m_next;
		while(scan != &m_end)
		{
			if ( test(reinterpret_cast<Node const*>(scan)->m_data) )
				scan = eraseAndNext(scan);
			else
				scan = scan->m_next;
		}
	}

	void unique()
	{
		for(NodeBase* scan = m_end.m_next; scan != &m_end; scan = scan->m_next)
			for(NodeBase* next = scan->m_next; next != &m_end; next = scan->m_next)
			{
				if (   reinterpret_cast<Node*>(scan)->m_data 
					== reinterpret_cast<Node*>(next)->m_data )
					delete unlink(next);
				else
					break;	// done on this element
			}
	}

	template <typename F>
	void unique( F equal )
	{
		for(NodeBase* scan = m_end.m_next; scan != &m_end; scan = scan->m_next)
			for(NodeBase* next = scan->m_next; next != &m_end; next = scan->m_next)
			{
				if (equal( reinterpret_cast<Node const*>(scan)->m_data 
						 , reinterpret_cast<Node const*>(next)->m_data ))
					delete unlink(next);
				else
					break;	// done on this element
			}
	}

	iterator find(const T& data) const
	{
		for(NodeBase* scan = m_end.m_next; scan != &m_end; scan = scan->m_next)
			if ( reinterpret_cast<Node*>(scan)->m_data == data )
				return iterator(scan);
		return end();
	}

	iterator find(iterator first, iterator last, const T& data) const
	{
		for(NodeBase* scan = first.m_node; scan != last.m_node; scan = scan->m_next)
			if ( reinterpret_cast<Node*>(scan)->m_data == data )
				return iterator(scan);
		return end();
	}

	template <typename F>
	iterator find_if( F test ) const
	{
		for(NodeBase* scan = m_end.m_next; scan != &m_end; scan = scan->m_next)
			if ( test(reinterpret_cast<Node const*>(scan)->m_data) )
				return iterator(scan);
		return end();
	}

	template <typename F>
	iterator find_if(iterator first, iterator last, F test ) const
	{
		for(NodeBase* scan = first.m_node; scan != last.m_node; scan = scan->m_next)
			if ( test(reinterpret_cast<Node const*>(scan)->m_data) )
				return iterator(scan);
		return end();
	}

	void clear()
	{
		while(m_end.m_next != &m_end)
			delete unlink(m_end.m_next);
	}

	~list()
	{
		clear();
	}

private:
	NodeBase* insertElement(const T &e, NodeBase* pos)
	{
		return insertNode(new Node(e), pos);
	}

	NodeBase* insertElement(T &&e, NodeBase* pos)
	{
		return insertNode(new Node(e), pos);
	}

	NodeBase* insertNode(NodeBase* node, NodeBase* pos)
	{
		++m_size;
		node->insert(pos);
		return node;
	}

	NodeBase* unlink(NodeBase* node)
	{
		--m_size;
		node->extract();
		return node;
	}

	NodeBase* eraseAndNext(NodeBase* node)
	{
		NodeBase* next = node->m_next;
		delete unlink(node);
		return next;
	}

	size_t   m_size{0};	///< counted at inserts and unlinks
	NodeBase m_end;		///< virtual Node, that links first and last and represents unique end() value
};

}

#endif
