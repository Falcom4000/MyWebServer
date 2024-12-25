
#include "gtest/gtest.h"
#include <thread>
#include "../src/timer/heaptimer.cpp"

// 测试添加计时器并获取下一个超时时间
TEST(HeapTimerTest, AddAndGetNextTimeout) {
    HeapTimer timer;
    timer.add(1, 1000, [](){});
    EXPECT_EQ(timer.GetNextTimeout(), 1000);
}

// 测试删除计时器
TEST(HeapTimerTest, DeleteTimer) {
    HeapTimer timer;
    timer.add(2, 2000, [](){});
    timer.del(2);
    // 验证计时器是否被删除
    EXPECT_EQ(timer.GetNextTimeout(), -1);
}

// 测试重置计时器
TEST(HeapTimerTest, ResetTimer) {
    HeapTimer timer;
    timer.add(3, 3000, [](){});
    timer.reset(3, 1500);
    EXPECT_EQ(timer.GetNextTimeout(), 1500);
}

// 测试完成计时器并触发回调
TEST(HeapTimerTest, DoneTimer) {
    bool callbackCalled = false;
    HeapTimer timer;
    timer.add(4, 4000, [&callbackCalled]() { callbackCalled = true; });
    timer.done(4);
    EXPECT_TRUE(callbackCalled);
}

// 测试 tick 函数处理过期和删除的计时器
TEST(HeapTimerTest, TickFunction) {
    bool callbackCalled = false;
    HeapTimer timer;
    timer.add(5, 10, [&callbackCalled]() { callbackCalled = true; });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    timer.tick();
    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(timer.GetNextTimeout(), -1);
}