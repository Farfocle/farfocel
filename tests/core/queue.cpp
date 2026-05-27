#include <doctest.h>

#include "fr/core/queue.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

TEST_CASE("Queue - Construction") {
    Queue<S32> queue;

    CHECK(queue.size() == 0);
    CHECK(queue.capacity() == 0);
    CHECK(queue.is_empty());
}

TEST_CASE("Queue - Enqueue and Dequeue") {
    Queue<S32> queue;

    queue.enqueue(10);
    queue.emplace_enqueue(20);
    queue.enqueue(30);

    CHECK(queue.size() == 3);
    CHECK(queue.front() == 10);

    queue.dequeue();
    CHECK(queue.size() == 2);
    CHECK(queue.front() == 20);

    queue.dequeue();
    CHECK(queue.size() == 1);
    CHECK(queue.front() == 30);

    queue.dequeue();
    CHECK(queue.size() == 0);
    CHECK(queue.is_empty());
}

TEST_CASE("Queue - FIFO order with wraparound") {
    Queue<S32> queue;

    queue.enqueue(1);
    queue.enqueue(2);
    queue.enqueue(3);
    queue.enqueue(4);

    CHECK(queue.front() == 1);
    queue.dequeue();
    CHECK(queue.front() == 2);
    queue.dequeue();

    queue.enqueue(5);
    queue.enqueue(6);

    CHECK(queue.front() == 3);
    queue.dequeue();
    CHECK(queue.front() == 4);
    queue.dequeue();
    CHECK(queue.front() == 5);
    queue.dequeue();
    CHECK(queue.front() == 6);
    queue.dequeue();

    CHECK(queue.is_empty());
}

TEST_CASE("Queue - Clear resets state") {
    Queue<S32> queue;

    queue.enqueue(7);
    queue.enqueue(8);
    queue.clear();

    CHECK(queue.size() == 0);
    CHECK(queue.is_empty());
}

} // namespace fr
