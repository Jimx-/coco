#include <gtest/gtest.h>
#include <unistd.h>

#include "coco/coco.h"

TEST(CocoTest, SpawnTasks)
{
    coco::go([] {
        for (int i = 0; i < 10; i++) {
            std::cout << "Hello " << i << std::endl;
            coco::yield();
        }
    });

    coco::go([] {
        for (int i = 0; i < 10; i++) {
            std::cout << "world " << i << std::endl;
            coco::yield();
        }
    });

    coco::run();
}

TEST(CocoTest, LoadBalance)
{
    for (int j = 0; j < 10; j++)
        coco::go(
            [j] {
                for (int i = 0; i < 10; i++) {
                    usleep(20000); // wait for load balancer to kick in
                    std::cout
                        << "Thread "
                        << coco::ThreadContext::get_current_thread()->get_tid()
                        << " Coroutine " << j << std::endl;
                    coco::yield();
                }
            });

    coco::run();
}

TEST(CocoTest, HandleException)
{
    coco::go([] { throw std::runtime_error("test"); });

    EXPECT_THROW({ coco::run(); }, std::runtime_error);
}

void test_stack_overflow()
{
    coco::yield();
    test_stack_overflow();
}

TEST(CocoTest, DetectStackOverflow)
{
    coco::go(test_stack_overflow);

    EXPECT_THROW({ coco::run(); }, std::runtime_error);
}
