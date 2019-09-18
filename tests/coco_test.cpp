#include <gtest/gtest.h>

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
