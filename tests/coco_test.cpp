#include <gtest/gtest.h>

#include "coco/scheduler.h"

TEST(CocoTest, SpawnTasks)
{
    SCHED.go([] {
        for (int i = 0; i < 10; i++) {
            std::cout << "Hello " << i << std::endl;
            SCHED.yield();
        }
    });

    SCHED.go([] {
        for (int i = 0; i < 10; i++) {
            std::cout << "world " << i << std::endl;
            SCHED.yield();
        }
    });

    SCHED.run();
}
