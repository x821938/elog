#ifndef ELOG_LOGRINGBUFF_H
#define ELOG_LOGRINGBUFF_H

#include <Arduino.h>
#include <freertos/semphr.h>

template <typename T>
class LogRingBuff {
public:
    bool buffCreate(size_t logLineCapacity);
    bool buffPush(const T& entry);
    bool buffPop(T& entry);
    bool buffIsEmpty() const;
    bool buffIsFull() const;
    size_t buffSize() const;
    size_t buffCapacity() const;
    uint8_t buffPercentageFull() const;

private:
    SemaphoreHandle_t semaphore;

    T* entries = nullptr;
    size_t size = 0;
    size_t front = 0;
    size_t rear = 0;
    size_t capacity = 0;
};

// Inline methods must be done inline in the header file in order for templates to work.

/* Create a ringbuffer with a given capacity of elements */
template <typename T>
bool LogRingBuff<T>::buffCreate(size_t capacity)
{
    if (entries != nullptr) {
        return false; // Already created
    }

    this->capacity = capacity;
    size = 0;
    front = 0;
    rear = 0;

    try {
        entries = new T[capacity];
    } catch (const std::bad_alloc& e) {
        return false; // Not enough memory
    }

    semaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(semaphore); // Initialize semaphore to available state
    return true;
}

/* Push an element to the ringbuffer */
template <typename T>
bool LogRingBuff<T>::buffPush(const T& entry)
{
    if (buffIsFull()) {
        return false;
    }

    xSemaphoreTake(semaphore, portMAX_DELAY);
    entries[rear] = entry; // Structs copied by assignment
    rear = (rear + 1) % capacity;
    size++;
    xSemaphoreGive(semaphore);
    return true;
}

/* Pop an element from the ringbuffer */
template <typename T>
bool LogRingBuff<T>::buffPop(T& entry)
{
    if (buffIsEmpty()) {
        return false;
    }

    xSemaphoreTake(semaphore, portMAX_DELAY);
    entry = entries[front];
    front = (front + 1) % capacity;
    size--;
    xSemaphoreGive(semaphore);
    return true;
}

/* Returns true if the ringbuffer is full */
template <typename T>
bool LogRingBuff<T>::buffIsFull() const
{
    return size == capacity;
}

/* Returns true if the ringbuffer is empty */
template <typename T>
bool LogRingBuff<T>::buffIsEmpty() const
{
    return size == 0;
}

/* Returns the number of elements in the ringbuffer */
template <typename T>
size_t LogRingBuff<T>::buffSize() const
{
    xSemaphoreTake(semaphore, portMAX_DELAY);
    size_t s = size;
    xSemaphoreGive(semaphore);
    return s;
}

/* Returns the maximum allowed elements in the ringbuffer */
template <typename T>
size_t LogRingBuff<T>::buffCapacity() const
{
    xSemaphoreTake(semaphore, portMAX_DELAY);
    size_t c = capacity;
    xSemaphoreGive(semaphore);
    return c;
}

/* Return how many percent of the ringbuffer is used. */
template <typename T>
uint8_t LogRingBuff<T>::buffPercentageFull() const
{
    xSemaphoreTake(semaphore, portMAX_DELAY);
    uint8_t ringBuffPercentage = size * 100 / capacity;
    xSemaphoreGive(semaphore);
    return ringBuffPercentage;
}

#endif // ELOG_LOGRINGBUFF_H
