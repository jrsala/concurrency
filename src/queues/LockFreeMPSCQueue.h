#ifndef LOCKFREEMPSCQUEUE_H
#define LOCKFREEMPSCQUEUE_H


#include <memory>
#include <atomic>
#include <type_traits>
#include <cassert>
#include "cache.h"


/**
 * Lock-free fixed-size contiguous strict FIFO multi-producer single consumer
 * queue
 */
template <class T, class Allocator = std::allocator<T>>
class LockFreeMPSCQueue
{
private:
	class Slot
	{
	public:
		enum class State
		{
			Empty,
			Writing,
			Occupied
		};

	public:
		Slot() :
			_state(State::Empty)
		{ }

		~Slot()
		{
			if (_state.load(std::memory_order_relaxed) == State::Occupied)
			{
				this->element()->~T();
			}
		}

		T * element()
		{
			return reinterpret_cast<T*>(&_element);
		}

		void clear()
		{
			if (_state.load(std::memory_order_relaxed) == State::Occupied)
			{
				this->element()->~T();
				_state.store(State::Empty, std::memory_order_relaxed);
			}
		}

	private:
		using storage_type = typename std::aligned_storage<sizeof(T), alignof(T)>::type;
		storage_type _element;

	public:
		std::atomic<State> _state;
	};

	using alloc_traits = std::allocator_traits<Allocator>;
	using slot_allocator = typename alloc_traits::template rebind_alloc<Slot>;
	using slot_alloc_traits = std::allocator_traits<slot_allocator>;

public:
	LockFreeMPSCQueue(
		size_t buffer_size_,
		const Allocator & alloc_ = Allocator()
	) :
		_slot_alloc(alloc_),
		_buffer(slot_alloc_traits::allocate(_slot_alloc, buffer_size_)),
		_buffer_size(buffer_size_),
		_modulo_mask(buffer_size_ - 1),
		_write_index(0),
		_read_index(0)
	{
		// Ensure buffer_size_ is greater than 1 and a power of two
		assert(buffer_size_ > 1 && ((buffer_size_ & _modulo_mask) == 0));

		for (size_t i = 0; i < _buffer_size; ++i)
		{
			slot_alloc_traits::construct(_slot_alloc, &_buffer[i]);
		}
	}

	~LockFreeMPSCQueue()
	{
		for (size_t i = 0; i < _buffer_size; ++i)
		{
			slot_alloc_traits::destroy(_slot_alloc, &_buffer[i]);
		}

		slot_alloc_traits::deallocate(_slot_alloc, _buffer, _buffer_size);
	}

	void enqueue(T && element_)
	{
		Slot * slot;

		for (;;)
		{
			size_t current_write_index = _write_index.load(std::memory_order_relaxed);
			size_t current_read_index = _read_index.load(std::memory_order_acquire);

			// Can't do anything if the queue is full
			if (distance(current_write_index, current_read_index) == 1)
			{
				__asm__("pause");
				continue;
			}

			slot = &_buffer[current_write_index];
			auto expected = Slot::State::Empty;

			if (slot->_state.compare_exchange_weak(
					expected,
					Slot::State::Writing,
					std::memory_order_relaxed
				))
			{
				if (_write_index.compare_exchange_weak(
						current_write_index,
						incremented(current_write_index),
						std::memory_order_relaxed
					))
				{
					break;
				}
				else
				{
					slot->_state.store(Slot::State::Empty, std::memory_order_relaxed);
				}
			}

			__asm__("pause");
		}

		new (slot->element()) T(std::move(element_));

		slot->_state.store(
			Slot::State::Occupied,
			std::memory_order_release
		);
	}

	T dequeue()
	{
		size_t current_read_index = _read_index.load(std::memory_order_relaxed);
		Slot & slot = _buffer[current_read_index];

		while (slot._state.load(std::memory_order_acquire) != Slot::State::Occupied)
		{
			__asm__("pause");
		}

		T result(std::move(*slot.element()));
		slot.element()->~T();

		std::atomic_thread_fence(std::memory_order_release);

		_read_index.store(incremented(current_read_index), std::memory_order_relaxed);
		slot._state.store(Slot::State::Empty, std::memory_order_relaxed);

		return result;
	}

	/**
	 * Not thread-safe!
	 */
	void clear()
	{
		for (size_t i = 0; i < _buffer_size; ++i)
		{
			_buffer[i].clear();
		}
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

private:
	slot_allocator _slot_alloc;
	typename slot_alloc_traits::pointer _buffer;
	size_t _buffer_size;
	size_t _modulo_mask;

	alignas(CACHE_LINE_SIZE)
	std::atomic<size_t> _write_index;

	alignas(CACHE_LINE_SIZE)
	std::atomic<size_t> _read_index;

	char cacheLinePadding[CACHE_LINE_SIZE - sizeof(_read_index)];
};


#endif // ifndef LOCKFREEMPSCQUEUE_H
