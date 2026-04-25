// Copyright 2021 GHA Test Team

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstdint>
#include <stdexcept>
#include "TimedDoor.h"

class MockTimerClient : public TimerClient {
  public:
    MOCK_METHOD(void, Timeout, (), (override));
};

class MockDoor : public Door {
  public:
    MOCK_METHOD(void, lock, (), (override));
    MOCK_METHOD(void, unlock, (), (override));
    MOCK_METHOD(bool, isDoorOpened, (), (override));
};

class TimedDoorTest : public ::testing::Test {
  protected:
    TimedDoor* door;
    static constexpr int TIMEOUT = 1;

    void SetUp() override {
        door = new TimedDoor(TIMEOUT);
    }

    void TearDown() override {
        delete door;
    }
};

TEST_F(TimedDoorTest, ConstructorInitializesDoorClosed) {
    EXPECT_FALSE(door->isDoorOpened());
}

TEST_F(TimedDoorTest, ConstructorSetsCorrectTimeout) {
    EXPECT_EQ(door->getTimeOut(), TIMEOUT);
}


TEST_F(TimedDoorTest, LockClosesTheDoor) {
    door->unlock();
    EXPECT_TRUE(door->isDoorOpened());
    door->lock();
    EXPECT_FALSE(door->isDoorOpened());
}

TEST_F(TimedDoorTest, UnlockOpensTheDoor) {
    door->lock();
    EXPECT_FALSE(door->isDoorOpened());
    door->unlock();
    EXPECT_TRUE(door->isDoorOpened());
}

TEST_F(TimedDoorTest, ThrowStateWhenDoorOpenThrowsException) {
    door->unlock();
    EXPECT_THROW(door->throwState(), std::runtime_error);
}

TEST_F(TimedDoorTest, ThrowStateWhenDoorClosedDoesNotThrow) {
    door->lock();
    EXPECT_NO_THROW(door->throwState());
}

TEST_F(TimedDoorTest, TimerCallsTimeoutAfterInterval) {
    MockTimerClient mockClient;
    Timer timer;
    EXPECT_CALL(mockClient, Timeout()).Times(1);
    timer.tregister(1, &mockClient);
}

TEST_F(TimedDoorTest, AdapterTimeoutCallsThrowStateWhenDoorOpen) {
    door->unlock();
    DoorTimerAdapter adapter(*door);
    EXPECT_THROW(adapter.Timeout(), std::runtime_error);
}

TEST_F(TimedDoorTest, AdapterTimeoutDoesNotThrowWhenDoorClosed) {
    door->lock();
    DoorTimerAdapter adapter(*door);

    EXPECT_NO_THROW(adapter.Timeout());
}

TEST(TimedDoorMockTest, MockDoorCanBeUsedForTesting) {
    MockDoor mockDoor;

    EXPECT_CALL(mockDoor, isDoorOpened())
        .WillOnce(::testing::Return(false));
    EXPECT_FALSE(mockDoor.isDoorOpened());
}

TEST(TimedDoorMockTest, UnlockCallsMockMethod) {
    MockDoor mockDoor;
    EXPECT_CALL(mockDoor, unlock()).Times(1);
    mockDoor.unlock();
}

TEST_F(TimedDoorTest, FullScenarioDoorClosedAfterTimeout) {
    door->unlock();
    door->lock();
    EXPECT_NO_THROW(door->throwState());
}

TEST_F(TimedDoorTest, FullScenarioDoorOpenAfterTimeoutThrows) {
    door->unlock();
    EXPECT_THROW(door->throwState(), std::runtime_error);
}

TEST_F(TimedDoorTest, MultipleLockUnlockCycles) {
    for (int i = 0; i < 5; i++) {
        door->unlock();
        EXPECT_TRUE(door->isDoorOpened());
        door->lock();
        EXPECT_FALSE(door->isDoorOpened());
    }
}

TEST_F(TimedDoorTest, ExceptionMessageIsCorrect) {
    door->unlock();
    try {
        door->throwState();
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Door is still open after timeout!");
    }
}
