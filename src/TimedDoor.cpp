// Copyright 2021 GHA Test Team
#include "TimedDoor.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

// Реализация адаптера
DoorTimerAdapter::DoorTimerAdapter(TimedDoor& targetDoor) : door(targetDoor) {}

void DoorTimerAdapter::Timeout() { door.throwState(); }

// Реализация TimedDoor
TimedDoor::TimedDoor(int secondsForTimeout)
    : iTimeout(secondsForTimeout), isOpened(false) {
  if (secondsForTimeout <= 0) {
    throw std::invalid_argument("Timeout must be positive");
  }
  adapter = new DoorTimerAdapter(*this);
}

TimedDoor::~TimedDoor() { delete adapter; }

bool TimedDoor::isDoorOpened() {
  std::lock_guard<std::mutex> guard(doorMutex);
  return isOpened;
}

void TimedDoor::unlock() {
  std::lock_guard<std::mutex> guard(doorMutex);
  isOpened = true;
}

void TimedDoor::lock() {
  std::lock_guard<std::mutex> guard(doorMutex);
  isOpened = false;
}

int TimedDoor::getTimeOut() const { return iTimeout; }

void TimedDoor::throwState() {
  std::lock_guard<std::mutex> guard(doorMutex);
  if (isOpened) {
    throw std::runtime_error("Door is still open after timeout!");
  }
}

// Вспомогательный класс для асинхронного таймера
class TimerImpl {
 private:
  std::atomic<bool> activeFlag{false};
  std::thread workerThread;

 public:
  void startTimer(int durationSeconds, TimerClient* callbackClient) {
    if (activeFlag.load()) {
      return;
    }

    activeFlag.store(true);
    workerThread = std::thread([this, durationSeconds, callbackClient]() {
      std::this_thread::sleep_for(std::chrono::seconds(durationSeconds));
      if (activeFlag.load() && callbackClient != nullptr) {
        callbackClient->Timeout();
      }
      activeFlag.store(false);
    });
  }

  void cancelTimer() {
    activeFlag.store(false);
    if (workerThread.joinable()) {
      workerThread.detach();
    }
  }

  ~TimerImpl() { cancelTimer(); }
};

// Глобальный экземпляр таймера
static TimerImpl globalTimerEngine;

void Timer::sleep(int seconds) {
  std::this_thread::sleep_for(std::chrono::seconds(seconds));
}

void Timer::tregister(int timeoutValue, TimerClient* clientHandler) {
  if (timeoutValue <= 0 || clientHandler == nullptr) {
    return;
  }

  globalTimerEngine.startTimer(timeoutValue, clientHandler);
}
