#include "queues/LockFreeMPSCQueue.h"

#include <chrono>
#include <iostream>
#include <thread>
#include "thread_affinity.h"


constexpr int PROCESSOR_COUNT = 4;

constexpr size_t PRODUCERS_COUNT = 3;
constexpr size_t PRODUCED_ELEMENTS_PER_PRODUCER = 1 << 24;

constexpr size_t ELEMENT_BLOAT_SIZE = 128;
constexpr size_t QUEUE_BUFFER_SIZE = 1 << 10;
constexpr size_t PRODUCED_ELEMENTS = PRODUCERS_COUNT * PRODUCED_ELEMENTS_PER_PRODUCER;
constexpr size_t EXPECTED_SUM = (PRODUCED_ELEMENTS - 1) * PRODUCED_ELEMENTS / 2;


struct Thing
{
	size_t x;
	char bloat[ELEMENT_BLOAT_SIZE];
};

template <class queue_t>
void produce(queue_t & queue_, size_t initial_value_)
{
	set_current_thread_affinity(initial_value_ % PROCESSOR_COUNT);

	for (size_t i = 0; i < PRODUCED_ELEMENTS_PER_PRODUCER; ++i)
	{
		Thing t;
		t.x = initial_value_ + i * PRODUCERS_COUNT;
		queue_.enqueue(std::move(t));
	}
}

template <class queue_t>
void consume(queue_t & queue_, size_t & result_)
{
	set_current_thread_affinity(PROCESSOR_COUNT - 1);

	size_t nb_dequeued_elements = 0;
	size_t total = 0;

	while (nb_dequeued_elements < PRODUCED_ELEMENTS)
	{
		total += queue_.dequeue().x;
		++nb_dequeued_elements;
	}

	result_ = total;
}

template <class queue_t>
void run_test(const char * queue_name_)
{
	queue_t queue(QUEUE_BUFFER_SIZE);
	size_t result;

	std::array<std::thread, PRODUCERS_COUNT> producer_threads;

	auto start_time = std::chrono::steady_clock::now();

	std::thread consumer_thread(consume<queue_t>, std::ref(queue), std::ref(result));

	for (size_t i = 0; i < PRODUCERS_COUNT; ++i)
	{
		producer_threads[i] = std::thread(produce<queue_t>, std::ref(queue), i);
	}

	consumer_thread.join();

	std::chrono::duration<double> elapsed_time =
		std::chrono::steady_clock::now() - start_time;

	for (auto & thread : producer_threads)
	{
		thread.join();
	}

	std::cout
		<< queue_name_ << " took "
		<< elapsed_time.count() << " seconds\n"
		   "Expected " << EXPECTED_SUM <<", got " << result
		<< ": " << (result == EXPECTED_SUM ? "" : "N") << "OK!\n";
}

int main()
{
	std::cout
		<< "Sending " << PRODUCED_ELEMENTS
		<< " objects of size " << sizeof(Thing) << " bytes"
		   " (total " << PRODUCED_ELEMENTS * sizeof(Thing)
		<< " bytes) through queue of capacity " << (QUEUE_BUFFER_SIZE - 1)
		<< " with " << PRODUCERS_COUNT << " producer threads\n";

	run_test<LockFreeMPSCQueue<Thing>>("LockFreeMPSCQueue");

	return 0;
}
