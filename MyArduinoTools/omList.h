#ifndef OM_LIST_H
#define OM_LIST_H

namespace om {

template<typename T>
class list
{
private:
	struct Node
	{
		T		data;
		Node*	m_prev {nullptr};
		Node*	m_next {nullptr};
		
		Node(const T &e)
			: data(e)
		{}

		Node(T &&e)
			: data(e)
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
			return m_node->data; 
		}

		T *operator->() const
		{ 
			return &(m_node->data); 
		}

		bool operator != (const iterator &rhs) const
		{ 
			return this->m_node != rhs.m_node; 
		}

		bool operator == (const iterator &rhs) const
		{ 
			return this->m_node != rhs.m_node; 
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

		iterator& operator += (unsigned int count)
		{
			while (m_node && (count-- > 0))
				m_node = m_node->m_next;
			return *this;
		}

		iterator& operator -= (unsigned int count)
		{
			while (m_node && (count-- > 0))
				m_node = m_node->m_prev;
			return *this;
		}

		iterator operator+(unsigned int count) const
		{
			iterator copy(*this);
			return copy += count;
		}

		iterator operator-(unsigned int count) const
		{
			iterator copy(*this);
			return copy -= count;
		}

	private:
		friend class list<T>;
		iterator(Node *ptr) 
			: m_node(ptr) 
		{}

		Node *m_node {nullptr};
	};

	list()
	{}

	list(const list<T> &other)
	{
		// make copy of each element
		for(Node* node = other.m_front; node; node = node->m_next)
			doInsert(node->data, nullptr);
	}

	list(list<T> &&other)
	{
		// steel away others elements
		m_size  = other.m_size;		other.m_size  = 0;
		m_front = other.m_front;	other.m_front = nullptr;
		m_back  = other.m_back;		other.m_back  = nullptr;
	}

	uint32_t size() const
	{
		return m_size;
	}

	void resize(uint32_t count)
	{
		while(m_size > count)
			delete unlink(m_back);
		while(m_size < count)
			doInsert(T{}, nullptr);
	}

	void resize(uint32_t count, const T& value)
	{
		while(m_size > count)
			delete unlink(m_back);
		while(m_size < count)
			doInsert(value, nullptr);
	}

	bool empty() const
	{
		return m_size == 0;
	}

	T& front()
	{
		return m_front->data;
	}

	T& back()
	{
		return m_back->data;
	}

	void push_front(const T &e)
	{
		doInsert(e, m_front);
	}

	void push_front(T &&e)
	{
		doInsert(e, m_front);
	}

	void push_back(const T &e)
	{
		doInsert(e, nullptr);
	}

	void push_back(T &&e)
	{
		doInsert(e, nullptr);
	}

	iterator insert(iterator pos, const T &e)
	{
		return iterator(doInsert(e, pos.m_node));
	}

	void insert(iterator pos, uint32_t count, const T &e)
	{
		while (count-- > 0)
			doInsert(e, pos.m_node);
	}

	void splice(iterator pos, list<T> &other)
	{
		while(other.m_size > 0)
			link(other.unlink(other.m_front), pos.m_node);
	}

	void splice(iterator pos, list<T> &other, iterator it)
	{
		link(other.unlink(it.m_node), pos.m_node);
	}

	void splice(iterator pos, list<T> &other, iterator first, iterator last)
	{
		while(first != last)
		{
			Node* node = other.unlink(first.m_node);
			++first; // still works, since node not deleted
			link(node, pos.m_node);
		}
	}

	void sort()
	{
		for (bool sorted = false; !sorted; )
		{
			sorted = true;	// give it a try
			Node* stopAtMax = nullptr;
			for (Node* scan = m_front; scan != stopAtMax; )
			{
				Node* next = scan->m_next;
				if (next && (next->data < scan->data))
				{
					// swap and move maximum to end
					link(unlink(next), scan);
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
	void sort( F comp )
	{
		for (bool sorted = false; !sorted; )
		{
			sorted = true;	// give it a try
			Node* stopAtMax = nullptr;
			for (Node* scan = m_front; scan != stopAtMax; )
			{
				Node* next = scan->m_next;
				if (next && comp(next->data, scan->data))
				{
					// swap and move maximum to end
					link(unlink(next), scan);
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
		delete unlink(m_front);
	}

	void pop_back()
	{
		delete unlink(m_back);
	}

	void erase(iterator pos)
	{
		delete unlink(pos.m_ptr);
	}

	void erase(iterator first, iterator last)
	{
		while (first != last)
		{
			Node* toRemove = first;
			++first;
			delete unlink(toRemove.m_ptr);
		}
	}

	void remove(const T& data)
	{
		Node* scan = m_front;
		while(scan)
		{
			if (scan->data == data)
			{
				Node* hold = unlink(scan);
				scan = hold->m_next;
				delete hold;
			}
			else
				scan = scan->m_next;
		}
	}

	template <typename F>
	void remove_if( F test )
	{
		Node* scan = m_front;
		while(scan)
		{
			if ( test(scan->data) )
			{
				Node* hold = unlink(scan);
				scan = hold->m_next;
				delete hold;
			}
			else
				scan = scan->m_next;
		}
	}

	iterator find(const T& data) const
	{
		for(Node* scan = m_front; scan; scan = scan->m_next)
			if ( scan->data == data )
				return iterator(scan);
		return iterator();
	}

	template <typename F>
	iterator find_if( F test ) const
	{
		for(Node* scan = m_front; scan; scan = scan->m_next)
			if ( test(scan->data) )
				return iterator(scan);
		return iterator();
	}

	void clear()
	{
		while(m_front)
			delete unlink(m_front);
	}

	~list()
	{
		clear();
	}

	iterator begin()
	{ 
		return iterator(m_front); 
	}

	iterator end()
	{ 
		return iterator(nullptr); 
	}

private:
	Node* doInsert(const T &e, Node *pos)
	{
		Node* node = new Node(e);
		return link(node, pos);
	}

	Node* doInsert(T &&e, Node *pos)
	{
		Node* node = new Node(e);
		return link(node, pos);
	}

	Node* link(Node *node, Node *pos)
	{
		++m_size;
		node->m_next = pos;
		if (pos)
		{
			node->m_prev = pos->m_prev;
			pos->m_prev  = node;
		}
		else
		{	// if pos == nil, push_back
			node->m_prev = m_back;
			m_back = node;
		}

		if (node->m_prev)
			node->m_prev->m_next = node;
		else
			m_front = node;

		return node;
	}

	Node* unlink(Node *node)
	{
		--m_size;
		if (node->m_prev)
			node->m_prev->m_next = node->m_next;
		else // was node at front
			m_front = node->m_next;

		if (node->m_next)
			node->m_next->m_prev = node->m_prev;
		else // was node at back
			m_back = node->m_prev;

		return node;
	}

	uint32_t	m_size	{0};
	Node*		m_front	{nullptr};
	Node*		m_back	{nullptr};
};

}

#endif