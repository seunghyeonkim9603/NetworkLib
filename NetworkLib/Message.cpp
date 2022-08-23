#include <memory>

#include "Message.h"

Message::Message()
    : Message(DEFAULT_SIZE)
{
}

Message::Message(int capacity)
    : mCapacity(capacity),
    mBuffer(new char[capacity]),
    mFront(mBuffer),
    mRear(mBuffer)
{
}

Message::Message(const Message& other)
    : mCapacity(other.mCapacity),
    mBuffer(new char[other.mCapacity]),
    mFront(mBuffer + (other.mFront - other.mBuffer)),
    mRear(mBuffer + (other.mRear - other.mBuffer))
{
    memcpy(mBuffer, other.mBuffer, mCapacity);
}

Message::Message(Message&& other) noexcept
    : mCapacity(other.mCapacity),
    mBuffer(other.mBuffer),
    mFront(other.mFront),
    mRear(other.mRear)
{
    other.mBuffer = nullptr;
}

Message::~Message()
{
    delete mBuffer;
}

