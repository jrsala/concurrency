#ifndef LOCKFREESPSCQUEUE_H
#define LOCKFREESPSCQUEUE_H


#include <memory>
#include <atomic>
#include <cassert>
#include "cache.h"


/**
 * Lock-free fixed-size contiguous strict FIFO single producer single consumer
 * queue
 */
template <class T, class Allocator = std::allocator<T>>
class LockFreeSPSCQueue
{
private:
	using alloc_traits = std::allocator_traits<Allocator>;

public:
	LockFreeSPSCQueue(
		size_t buffer_size_,
		const Allocator & alloc_ = Allocator()
	) :
		_alloc(alloc_),
		_buffer(alloc_traits::allocate(_alloc, buffer_size_)),
		_buffer_size(buffer_size_),
		_modulo_mask(buffer_size_ - 1),
		_write_index(0),
		_read_index(0)
	{
		// Ensure buffer_size_ is greater than 1 and a power of two
		assert(buffer_size_ > 1 && ((buffer_size_ & _modulo_mask) == 0));
	}

	LockFreeSPSCQueue(const LockFreeSPSCQueue &) = delete;
	LockFreeSPSCQueue & operator=(const LockFreeSPSCQueue &) = delete;

	~LockFreeSPSCQueue()
	{
		this->destroy_elements();
		alloc_traits::deallocate(_alloc, _buffer, _buffer_size);
	}

	void enqueue(T && element_)
	{
		size_t current_write_index = _write_index.load(std::memory_order_relaxed);

		for (;;)
		{
			size_t current_read_index = _read_index.load(std::memory_order_acquire);

			// Leave the loop if there is room in the queue
			if (distance(current_read_index, current_write_index) != _modulo_mask)
				break;

			__asm__("pause");
		}

		alloc_traits::construct(
			_alloc,
			&_buffer[current_write_index],
			std::move(element_)
		);

		_write_index.store(
			incremented(current_write_index),
			std::memory_order_release
		);
	}

	T dequeue()
	{
		size_t current_read_index = _read_index.load(std::memory_order_relaxed);
		T & element = _buffer[current_read_index];

		for (;;)
		{
			size_t current_write_index = _write_index.load(std::memory_order_acquire);

			// Leave the loop if there is an element in the queue
			if (current_write_index != current_read_index)
				break;

			__asm__("pause");
		}

		T result(std::move(element));
		alloc_traits::destroy(_alloc, &element);

		_read_index.store(
			incremented(current_read_index),
			std::memory_order_release
		);

		return result;
	}

	/**
	 * Not thread-safe!
	 */
	void clear()
	{
		this->destroy_elements();
		_read_index.store(0, std::memory_order_relaxed);
		_write_index.store(0, std::memory_order_relaxed);
	}

private:
	size_t distance(size_t from_, size_t to_) const
	{
		return (to_ - from_ + _buffer_size) & _modulo_mask;
	}

	size_t incremented(size_t index_) const
	{
		return (index_ + 1) & _modulo_mask;
	}

	void destroy_elements()
	{
		for (auto i = _read_index.load(std::memory_order_relaxed),
				  n = _write_index.load(std::memory_order_relaxed);
			 i < n;
			 ++i)
		{
			alloc_traits::destroy(_alloc, &_buffer[i]);
		}
	}

private:
	Allocator _alloc;
	typename Allocator::pointer _buffer;
	size_t _buffer_size;
	size_t _modulo_mask;

	alignas(CACHE_LINE_SIZE)
	std::atomic<size_t> _write_index;

	alignas(CACHE_LINE_SIZE)
	std::atomic<size_t> _read_index;

	char cacheLinePadding[CACHE_LINE_SIZE - sizeof(_read_index)];
};


#endif // ifndef LOCKFREESPSCQUEUE_H
