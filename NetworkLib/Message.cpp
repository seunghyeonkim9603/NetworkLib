#include "stdafx.h"

Message::Message()
    : Message(DEFAULT_SIZE)
{
}

Message::Message(int capacity)
    : mCapacity(capacity),
    mBuffer(new char[capacity]),
    mFront(mBuffer),
    mRear(mBuffer),
    mRefCount(new unsigned int(0))
{
}

Message::Message(const Message& other)
    : mCapacity(other.mCapacity),
    mBuffer(new char[other.mCapacity]),
    mFront(mBuffer + (other.mFront - other.mBuffer)),
    mRear(mBuffer + (other.mRear - other.mBuffer)),
    mRefCount(new unsigned int(*other.mRefCount))
{
    memcpy(mBuffer, other.mBuffer, mCapacity);
}

Message::Message(Message&& other) noexcept
    : mCapacity(other.mCapacity),
    mBuffer(other.mBuffer),
    mFront(other.mFront),
    mRear(other.mRear),
    mRefCount(other.mRefCount)
{
    other.mBuffer = nullptr;
    other.mRefCount = nullptr;
}

Message::~Message()
{
    delete mBuffer;
    delete mRefCount;
}

