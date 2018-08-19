#include "queues/LockFreeMPSCQueue.h"

#include <chrono>
#include <iostream>
#include <thread>
#include <stdexcept>
#include "thread_affinity.h"


constexpr size_t ELEMENT_BLOAT_SIZE = 128;
constexpr size_t QUEUE_BUFFER_SIZE = 1 << 10;
constexpr size_t PRODUCED_ELEMENTS = 50331648;


struct Thing
{
	size_t x;
	char bloat[ELEMENT_BLOAT_SIZE];
};

template <class queue_t>
void produce(queue_t & queue_)
{
	set_current_thread_affinity(0);

	for (size_t i = 0; i < PRODUCED_ELEMENTS; ++i)
	{
		Thing t;
		t.x = i;
		queue_.enqueue(std::move(t));
	}
}

template <class queue_t>
void consume(queue_t & queue_)
{
	set_current_thread_affinity(1);

	size_t i = 0;

	while (i < PRODUCED_ELEMENTS - 1)
	{
		size_t element = queue_.dequeue().x;

		if (element != i)
		{
			throw std::runtime_error("Buggy queue!");
		}

		++i;
	}
}

template <class queue_t>
void run_test(const char * queue_name_)
{
	queue_t queue(QUEUE_BUFFER_SIZE);

	auto start_time = std::chrono::steady_clock::now();

	std::thread consumer_thread(consume<queue_t>, std::ref(queue));
	std::thread producer_thread(produce<queue_t>, std::ref(queue));

	consumer_thread.join();

	std::chrono::duration<double> elapsedTime =
		std::chrono::steady_clock::now() - start_time;

	producer_thread.join();

	std::cout
		<< queue_name_ << " took "
		<< elapsedTime.count() << " seconds\n";
}

int main()
{
	std::cout
		<< "Sending " << PRODUCED_ELEMENTS
		<< " objects of size " << sizeof(Thing) << " bytes"
		   " (total " << PRODUCED_ELEMENTS * sizeof(Thing)
		<< " bytes) through queue of capacity " << (QUEUE_BUFFER_SIZE - 1)
		<< '\n';

	run_test<LockFreeMPSCQueue<Thing>>("LockFreeMPSCQueue");

	return 0;
}
