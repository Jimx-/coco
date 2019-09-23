#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include "coco/coco.h"
#include "coco/sync.h"

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

TEST(CocoTest, HandleIO)
{
    int fds[2];
    ASSERT_EQ(pipe(fds), 0);

    std::string result;
    coco::go([fds, &result] {
        char buf[10];
        read(fds[0], buf, 9);
        result = std::string(buf);
    });

    coco::go([fds] {
        sleep(1);
        char buf[] = "test";
        int n = write(fds[1], buf, 4);
    });

    coco::run();

    ASSERT_EQ(result, "test");
}

TEST(CocoTest, ConditionmVariable)
{
    coco::ConditionVariableAny cv;
    coco::SpinLock spl;
    int a = 0, b = 0;

    coco::go([&cv, &spl, &a, &b] {
        spl.lock();

        while (a == 0)
            cv.wait(spl);

        b = a;
        spl.unlock();
    });

    coco::go([&cv, &spl, &a] {
        sleep(1);

        spl.lock();
        a = 1;
        cv.notify_all();
        spl.unlock();
    });

    coco::run();

    ASSERT_EQ(b, 1);
}

TEST(CocoTest, Mutex)
{
    coco::Mutex mutex;
    int a = 0;

    for (int i = 0; i < 100; i++) {
        coco::go([&mutex, &a] {
            usleep(1000); // wait for load balancer to distribute the tasks to
                          // different threads

            std::lock_guard<coco::Mutex> lock(mutex);
            a++;
        });
    }

    coco::run();

    ASSERT_EQ(a, 100);
}

TEST(CocoTest, SharedMutex)
{
    coco::SharedMutex mutex;
    int a = 0;

    coco::go([&mutex] {
        for (int i = 0; i < 100; i++) {
            usleep(1000);
            mutex.lock_shared();
        }

        for (int i = 0; i < 100; i++) {
            mutex.unlock_shared();
        }
    });

    for (int i = 0; i < 100; i++) {
        coco::go([&mutex, &a] {
            usleep(1000); // wait for load balancer to distribute the tasks to
                          // different threads

            std::unique_lock<coco::SharedMutex> lock(mutex);
            a++;
        });
    }

    coco::run();

    ASSERT_EQ(a, 100);
}
