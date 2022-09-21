#pragma once

template<typename T>
class IntrusivePointer final
{
public:
	IntrusivePointer();
	IntrusivePointer(T* ptr);
	IntrusivePointer(IntrusivePointer<T>& other);
	~IntrusivePointer();

	T& operator*();
	T* operator->();
	IntrusivePointer<T>& operator=(IntrusivePointer<T>& other);
	
	void AddRefCount();
	void Release();

private:
	T*				mPtr;
	unsigned int*	mRefCount;
};

template<typename T>
inline IntrusivePointer<T>::IntrusivePointer()
	:	mPtr(nullptr),
		mRefCount(nullptr)
{
}

template<typename T>
inline IntrusivePointer<T>::IntrusivePointer(T* ptr)
	:	mPtr(ptr),
		mRefCount(new unsigned int(1))
{
}

template<typename T>
inline IntrusivePointer<T>::IntrusivePointer(IntrusivePointer<T>& other)
	:	mPtr(other.mPtr),
		mRefCount(other.mRefCount)
{
}

template<typename T>
inline IntrusivePointer<T>::~IntrusivePointer()
{
}

template<typename T>
inline T& IntrusivePointer<T>::operator*()
{
	return *mPtr;
}

template<typename T>
inline T* IntrusivePointer<T>::operator->()
{
	return mPtr;
}

template<typename T>
inline IntrusivePointer<T>& IntrusivePointer<T>::operator=(IntrusivePointer<T>& other)
{
	mPtr = other.mPtr;
	mRefCount = other.mRefCount;

	return *this;
}

template<typename T>
inline void IntrusivePointer<T>::AddRefCount()
{
	InterlockedIncrement(mRefCount);
}

template<typename T>
inline void IntrusivePointer<T>::Release()
{
	if (InterlockedDecrement(mRefCount) == 0)
	{
		delete mPtr;
		delete mRefCount;
	}
	mPtr = nullptr;
	mRefCount = nullptr;
}
