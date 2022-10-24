#pragma once

class Message final
{
    friend class Chunk<Message>;
    friend class WanServer;
    friend class LanServer;
public:
	inline void     Reserve(int capacity);
	inline void     Clear();
	inline int		GetCapacity() const;
    inline int		GetSize() const;
    inline char*    GetBuffer();
    inline char*	GetFront() const;
    inline char*    GetRear() const;
    inline int		MoveWritePos(int offset);
    inline int		MoveReadPos(int offset);

	inline Message& operator=(const Message& other);
	inline Message& operator=(Message&& other) noexcept;

	inline Message& operator<<(unsigned char val);
	inline Message& operator<<(char val);
	inline Message& operator<<(bool val);
	inline Message& operator<<(short val);
	inline Message& operator<<(unsigned short val);
	inline Message& operator<<(int val);
	inline Message& operator<<(unsigned int val);
	inline Message& operator<<(long val);
	inline Message& operator<<(unsigned long val);
	inline Message& operator<<(long long val);
	inline Message& operator<<(unsigned long long val);
	inline Message& operator<<(float val);
	inline Message& operator<<(double val);

	inline Message& operator>>(unsigned char& val);
	inline Message& operator>>(char& val);
	inline Message& operator>>(bool& val);
	inline Message& operator>>(short& val);
	inline Message& operator>>(unsigned short& val);
	inline Message& operator>>(int& val);
	inline Message& operator>>(unsigned int& val);
	inline Message& operator>>(long& val);
	inline Message& operator>>(unsigned long& val);
	inline Message& operator>>(long long& val);
	inline Message& operator>>(unsigned long long& val);
	inline Message& operator>>(float& val);
	inline Message& operator>>(double& val);

	inline Message& Write(const char* str, int size);
	inline Message& Read(char* outBuffer, int size);

    inline void AddReferenceCount();

private:
    static Message* Create();
    static void     Release(Message* message);

    Message();
    Message(int capacity);
    Message(const Message& other);
    Message(Message&& other) noexcept;
    ~Message();

    inline bool trySetEncodeFlag(bool bEncoded);
	inline int getEnquableSize() const;

private:
	enum { DEFAULT_SIZE = 2048 };

    static TLSObjectPool<Message>   MessagePool;

	int             mCapacity;
	char*           mBuffer;
	char*           mFront;
	char*           mRear;
    unsigned int*   mRefCount;
    bool            mbEncoded;
};

inline void Message::Reserve(int capacity)
{
    if (capacity <= mCapacity)
    {
        return;
    }
    Message temp(capacity);

    temp.Write(mFront, GetSize());

    *this = std::move(temp);
}

inline void Message::Clear()
{
    mFront = mBuffer;
    mRear = mBuffer; 
}

inline int Message::GetCapacity() const
{
    return mCapacity;
}

inline int Message::GetSize() const
{
    return static_cast<int>(mRear - mFront);
}

inline char* Message::GetBuffer()
{
    return mBuffer;
}

inline char* Message::GetRear() const
{
    return mRear;
}

inline char* Message::GetFront() const
{
    return mFront;
}

inline int Message::MoveWritePos(int offset)
{
    mRear += offset;
    return offset;
}

inline int Message::MoveReadPos(int offset)
{
    mFront += offset;
    return offset;
}

inline Message& Message::operator=(const Message& other)
{
    if (mBuffer != nullptr)
    {
        delete mBuffer;
    }
    mBuffer = new char[other.mCapacity];
    memcpy(mBuffer, other.mBuffer, other.mCapacity);
    mCapacity = other.mCapacity;
    mFront = mBuffer + (other.mFront - other.mBuffer);
    mRear = mBuffer + (other.mRear - other.mBuffer);

    return *this;
}

inline Message& Message::operator=(Message&& other) noexcept
{
    if (mBuffer != nullptr)
    {
        delete mBuffer;
    }
    mBuffer = other.mBuffer;
    mCapacity = other.mCapacity;
    mFront = other.mFront;
    mRear = other.mRear;

    delete other.mBuffer;
    other.mBuffer = nullptr;

    return *this;
}

inline Message& Message::operator<<(unsigned char val)
{
    int enquableSize = getEnquableSize();

    if (enquableSize < sizeof(val))
    {
        Reserve(mCapacity * 2);
    }
    unsigned char* rear = reinterpret_cast<unsigned char*>(mRear);

    *rear = val;
    ++rear;
    mRear = reinterpret_cast<char*>(rear);

    return *this;
}

inline Message& Message::operator<<(char val)
{
    int enquableSize = getEnquableSize();

    if (enquableSize < sizeof(val))
    {
        Reserve(mCapacity * 2);
    }
    char* rear = reinterpret_cast<char*>(mRear);

    *rear = val;
    ++rear;
    mRear = reinterpret_cast<char*>(rear);

    return *this;
}

inline Message& Message::operator<<(bool val)
{
    int enquableSize = getEnquableSize();

    if (enquableSize < sizeof(val))
    {
        Reserve(mCapacity * 2);
    }
    bool* rear = reinterpret_cast<bool*>(mRear);

    *rear = val;
    ++rear;
    mRear = reinterpret_cast<char*>(rear);

    return *this;
}

inline Message& Message::operator<<(short val)
{
    int enquableSize = getEnquableSize();

    if (enquableSize < sizeof(val))
    {
        Reserve(mCapacity * 2);
    }
    short* rear = reinterpret_cast<short*>(mRear);

    *rear = val;
    ++rear;
    mRear = reinterpret_cast<char*>(rear);

    return *this;
}

inline Message& Message::operator<<(unsigned short val)
{
    int enquableSize = getEnquableSize();

    if (enquableSize < sizeof(val))
    {
        Reserve(mCapacity * 2);
    }
    unsigned short* rear = reinterpret_cast<unsigned short*>(mRear);

    *rear = val;
    ++rear;
    mRear = reinterpret_cast<char*>(rear);

    return *this;
}

inline Message& Message::operator<<(int val)
{
    int enquableSize = getEnquableSize();

    if (enquableSize < sizeof(val))
    {
        Reserve(mCapacity * 2);
    }
    int* rear = reinterpret_cast<int*>(mRear);

    *rear = val;
    ++rear;
    mRear = reinterpret_cast<char*>(rear);

    return *this;
}

inline Message& Message::operator<<(unsigned int val)
{
    int enquableSize = getEnquableSize();

    if (enquableSize < sizeof(val))
    {
        Reserve(mCapacity * 2);
    }
    unsigned int* rear = reinterpret_cast<unsigned int*>(mRear);

    *rear = val;
    ++rear;
    mRear = reinterpret_cast<char*>(rear);

    return *this;
}

inline Message& Message::operator<<(long val)
{
    int enquableSize = getEnquableSize();

    if (enquableSize < sizeof(val))
    {
        Reserve(mCapacity * 2);
    }
    long* rear = reinterpret_cast<long*>(mRear);

    *rear = val;
    ++rear;
    mRear = reinterpret_cast<char*>(rear);

    return *this;
}

inline Message& Message::operator<<(unsigned long val)
{
    int enquableSize = getEnquableSize();

    if (enquableSize < sizeof(val))
    {
        Reserve(mCapacity * 2);
    }
    unsigned long* rear = reinterpret_cast<unsigned long*>(mRear);

    *rear = val;
    ++rear;
    mRear = reinterpret_cast<char*>(rear);

    return *this;
}

inline Message& Message::operator<<(long long val)
{
    int enquableSize = getEnquableSize();

    if (enquableSize < sizeof(val))
    {
        Reserve(mCapacity * 2);
    }
    long long* rear = reinterpret_cast<long long*>(mRear);

    *rear = val;
    ++rear;
    mRear = reinterpret_cast<char*>(rear);

    return *this;
}

inline Message& Message::operator<<(unsigned long long val)
{
    int enquableSize = getEnquableSize();

    if (enquableSize < sizeof(val))
    {
        Reserve(mCapacity * 2);
    }
    unsigned long long* rear = reinterpret_cast<unsigned long long*>(mRear);

    *rear = val;
    ++rear;
    mRear = reinterpret_cast<char*>(rear);

    return *this;
}

inline Message& Message::operator<<(float val)
{
    int enquableSize = getEnquableSize();

    if (enquableSize < sizeof(val))
    {
        Reserve(mCapacity * 2);
    }
    float* rear = reinterpret_cast<float*>(mRear);

    *rear = val;
    ++rear;
    mRear = reinterpret_cast<char*>(rear);

    return *this;
}

inline Message& Message::operator<<(double val)
{
    int enquableSize = getEnquableSize();

    if (enquableSize < sizeof(val))
    {
        Reserve(mCapacity * 2);
    }
    double* rear = reinterpret_cast<double*>(mRear);

    *rear = val;
    ++rear;
    mRear = reinterpret_cast<char*>(rear);

    return *this;
}

inline Message& Message::operator>>(unsigned char& outVal)
{
    outVal = *reinterpret_cast<unsigned char*>(mFront);
    mFront += sizeof(outVal);

    return *this;
}

inline Message& Message::operator>>(char& outVal)
{
    outVal = *reinterpret_cast<char*>(mFront);
    mFront += sizeof(outVal);

    return *this;
}

inline Message& Message::operator>>(bool& outVal)
{
    outVal = *reinterpret_cast<bool*>(mFront);
    mFront += sizeof(outVal);

    return *this;
}

inline Message& Message::operator>>(short& outVal)
{
    outVal = *reinterpret_cast<short*>(mFront);
    mFront += sizeof(outVal);

    return *this;
}

inline Message& Message::operator>>(unsigned short& outVal)
{
    outVal = *reinterpret_cast<unsigned short*>(mFront);
    mFront += sizeof(outVal);

    return *this;
}

inline Message& Message::operator>>(int& outVal)
{
    outVal = *reinterpret_cast<int*>(mFront);
    mFront += sizeof(outVal);

    return *this;
}

inline Message& Message::operator>>(unsigned int& outVal)
{
    outVal = *reinterpret_cast<unsigned int*>(mFront);
    mFront += sizeof(outVal);

    return *this;
}

inline Message& Message::operator>>(long& outVal)
{
    outVal = *reinterpret_cast<long*>(mFront);
    mFront += sizeof(outVal);

    return *this;
}

inline Message& Message::operator>>(unsigned long& outVal)
{
    outVal = *reinterpret_cast<unsigned long*>(mFront);
    mFront += sizeof(outVal);

    return *this;
}

inline Message& Message::operator>>(long long& outVal)
{
    outVal = *reinterpret_cast<long long*>(mFront);
    mFront += sizeof(outVal);

    return *this;
}

inline Message& Message::operator>>(unsigned long long& outVal)
{
    outVal = *reinterpret_cast<unsigned long long*>(mFront);
    mFront += sizeof(outVal);

    return *this;
}

inline Message& Message::operator>>(float& outVal)
{
    outVal = *reinterpret_cast<float*>(mFront);
    mFront += sizeof(outVal);

    return *this;
}

inline Message& Message::operator>>(double& outVal)
{
    outVal = *reinterpret_cast<double*>(mFront);
    mFront += sizeof(outVal);

    return *this;
}

inline Message& Message::Write(const char* str, int size)
{
    int enquableSize = getEnquableSize();

    if (enquableSize < size)
    {
        Reserve(mCapacity * 2);
    }
    memcpy(mRear, str, size);
    mRear += size;

    return *this;
}

inline Message& Message::Read(char* outBuffer, int size)
{
    memcpy(outBuffer, mFront, size);
    mFront += size;

    return *this;
}

inline void Message::AddReferenceCount()
{
    InterlockedIncrement(mRefCount);
}

inline Message* Message::Create()
{
    Message* message = MessagePool.GetObject();

    *message->mRefCount = 1;

    return message;
}

inline void Message::Release(Message* message)
{
    if (InterlockedDecrement(message->mRefCount) == 0)
    {
        message->Clear();
        MessagePool.ReleaseObject(message);
    }
}

inline bool Message::trySetEncodeFlag(bool bEncoded)
{
    return InterlockedExchange8((char*)&mbEncoded, bEncoded) != (char)bEncoded;
}

inline int Message::getEnquableSize() const
{
    return static_cast<int>(mBuffer + mCapacity - mRear);
}
