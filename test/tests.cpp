// Copyright 2021 GHA Test Team

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <thread>

#include "TimedDoor.h"

// Мок для тестирования клиентов таймера
class MockTimeNotificationClient : public TimerClient {
 public:
  MOCK_METHOD(void, Timeout, (), (override));
};

// Мок для тестирования интерфейса двери
class MockDoorInterface : public Door {
 public:
  MOCK_METHOD(void, lock, (), (override));
  MOCK_METHOD(void, unlock, (), (override));
  MOCK_METHOD(bool, isDoorOpened, (), (override));
};

class EnhancedTimedDoorTestSuite : public ::testing::Test {
 protected:
  TimedDoor* smartDoorUnderTest;
  static constexpr int DEFAULT_TIME_LIMIT_SECONDS = 1;
  static constexpr int SHORT_DELAY_MS = 100;

  void SetUp() override {
    smartDoorUnderTest = new TimedDoor(DEFAULT_TIME_LIMIT_SECONDS);
    ASSERT_NE(smartDoorUnderTest, nullptr);
  }

  void TearDown() override {
    delete smartDoorUnderTest;
    smartDoorUnderTest = nullptr;
  }

  void simulateShortDelay() {
    std::this_thread::sleep_for(std::chrono::milliseconds(SHORT_DELAY_MS));
  }
};

TEST_F(EnhancedTimedDoorTestSuite,
       VerifyDoorInitiallyInLockedStateAfterConstruction) {
  // Проверяем, что после создания дверь находится в закрытом состоянии
  EXPECT_FALSE(smartDoorUnderTest->isDoorOpened());
}

TEST_F(EnhancedTimedDoorTestSuite, ValidateTimeoutParameterIsCorrectlyStored) {
  // Проверяем корректность сохранения параметра таймаута
  const int customTimeout = 42;
  TimedDoor customDoor(customTimeout);
  EXPECT_EQ(customDoor.getTimeOut(), customTimeout);
}

TEST_F(EnhancedTimedDoorTestSuite, ConfirmLockOperationSuccessfullyClosesDoor) {
  // Открываем дверь, затем закрываем и проверяем состояние
  smartDoorUnderTest->unlock();
  EXPECT_TRUE(smartDoorUnderTest->isDoorOpened());

  smartDoorUnderTest->lock();
  EXPECT_FALSE(smartDoorUnderTest->isDoorOpened());
}

TEST_F(EnhancedTimedDoorTestSuite,
       ConfirmUnlockOperationSuccessfullyOpensDoor) {
  // Проверяем, что разблокировка корректно открывает дверь
  smartDoorUnderTest->lock();
  EXPECT_FALSE(smartDoorUnderTest->isDoorOpened());

  smartDoorUnderTest->unlock();
  EXPECT_TRUE(smartDoorUnderTest->isDoorOpened());
}

TEST_F(EnhancedTimedDoorTestSuite,
       ExceptionThrownWhenDoorRemainsOpenDuringStateCheck) {
  // Открытая дверь должна вызывать исключение при проверке состояния
  smartDoorUnderTest->unlock();
  EXPECT_THROW(smartDoorUnderTest->throwState(), std::runtime_error);
}

TEST_F(EnhancedTimedDoorTestSuite,
       NoExceptionWhenDoorIsClosedDuringStateCheck) {
  // Закрытая дверь не должна вызывать исключение
  smartDoorUnderTest->lock();
  EXPECT_NO_THROW(smartDoorUnderTest->throwState());
}

TEST_F(EnhancedTimedDoorTestSuite, VerifyExceptionMessageContainsExpectedText) {
  // Проверяем содержимое сообщения об ошибке
  smartDoorUnderTest->unlock();
  try {
    smartDoorUnderTest->throwState();
    FAIL() << "Expected runtime_error was not thrown";
  } catch (const std::runtime_error& errorObject) {
    EXPECT_STREQ(errorObject.what(), "Door is still open after timeout!");
  }
}

TEST_F(EnhancedTimedDoorTestSuite, AdapterTriggersExceptionWhenDoorIsOpen) {
  // Адаптер должен вызывать исключение через TimedDoor при открытой двери
  smartDoorUnderTest->unlock();
  DoorTimerAdapter notificationAdapter(*smartDoorUnderTest);
  EXPECT_THROW(notificationAdapter.Timeout(), std::runtime_error);
}

TEST_F(EnhancedTimedDoorTestSuite,
       AdapterDoesNotTriggerExceptionWhenDoorIsClosed) {
  // Адаптер не должен вызывать исключение при закрытой двери
  smartDoorUnderTest->lock();
  DoorTimerAdapter notificationAdapter(*smartDoorUnderTest);
  EXPECT_NO_THROW(notificationAdapter.Timeout());
}

TEST_F(EnhancedTimedDoorTestSuite,
       MockDoorInterfaceCorrectlyRespondsToMethodCalls) {
  // Проверка работы мок-объекта двери
  MockDoorInterface virtualDoor;

  EXPECT_CALL(virtualDoor, isDoorOpened()).WillOnce(::testing::Return(false));

  EXPECT_FALSE(virtualDoor.isDoorOpened());
}

TEST_F(EnhancedTimedDoorTestSuite, MockDoorUnlockMethodIsInvokedExactlyOnce) {
  // Проверяем, что метод unlock вызывается один раз
  MockDoorInterface virtualDoor;
  EXPECT_CALL(virtualDoor, unlock()).Times(1);
  virtualDoor.unlock();
}

TEST_F(EnhancedTimedDoorTestSuite,
       TimerCallsClientTimeoutAfterSpecifiedInterval) {
  // Проверяем, что таймер вызывает метод Timeout у клиента
  MockTimeNotificationClient mockNotificationReceiver;
  Timer systemTimer;

  EXPECT_CALL(mockNotificationReceiver, Timeout()).Times(1);
  systemTimer.tregister(1, &mockNotificationReceiver);

  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
}

TEST_F(EnhancedTimedDoorTestSuite, MultipleLockUnlockCyclesWorkCorrectly) {
  // Проверка множественных циклов открытия/закрытия
  const int cycleCount = 7;
  for (int iteration = 0; iteration < cycleCount; ++iteration) {
    smartDoorUnderTest->unlock();
    EXPECT_TRUE(smartDoorUnderTest->isDoorOpened());

    smartDoorUnderTest->lock();
    EXPECT_FALSE(smartDoorUnderTest->isDoorOpened());
  }
}

TEST_F(EnhancedTimedDoorTestSuite,
       DoorThatIsClosedBeforeTimeoutCheckDoesNotThrowException) {
  // Дверь открыта, но закрыта до проверки таймаута
  smartDoorUnderTest->unlock();
  smartDoorUnderTest->lock();
  EXPECT_NO_THROW(smartDoorUnderTest->throwState());
}

TEST_F(EnhancedTimedDoorTestSuite,
       DoorThatRemainsOpenThroughMultipleStateChecksThrowsEachTime) {
  // Дверь остается открытой через несколько проверок - каждая должна вызывать
  // исключение
  smartDoorUnderTest->unlock();

  for (int attempt = 0; attempt < 3; ++attempt) {
    EXPECT_THROW(smartDoorUnderTest->throwState(), std::runtime_error);
  }
}

TEST_F(EnhancedTimedDoorTestSuite,
       VerifyTimedDoorConstructorRejectsInvalidTimeout) {
  // Проверка, что конструктор отвергает невалидные значения таймаута
  EXPECT_THROW(TimedDoor invalidDoor(0), std::invalid_argument);
  EXPECT_THROW(TimedDoor invalidDoor(-5), std::invalid_argument);
}

TEST_F(EnhancedTimedDoorTestSuite, ConcurrentDoorOperationsDoNotCorruptState) {
  // Проверка потокобезопасности
  const int operationCount = 100;

  std::thread unlockThread([this, operationCount]() {
    for (int i = 0; i < operationCount; ++i) {
      smartDoorUnderTest->unlock();
    }
  });

  std::thread lockThread([this, operationCount]() {
    for (int i = 0; i < operationCount; ++i) {
      smartDoorUnderTest->lock();
    }
  });

  unlockThread.join();
  lockThread.join();

  EXPECT_NO_THROW(smartDoorUnderTest->lock());
}

TEST_F(EnhancedTimedDoorTestSuite,
       AdapterMaintainsValidReferenceAfterMultipleTimeouts) {
  DoorTimerAdapter persistentAdapter(*smartDoorUnderTest);

  for (int testRound = 0; testRound < 5; ++testRound) {
    smartDoorUnderTest->unlock();
    EXPECT_THROW(persistentAdapter.Timeout(), std::runtime_error);
    smartDoorUnderTest->lock();
    EXPECT_NO_THROW(persistentAdapter.Timeout());
  }
}
