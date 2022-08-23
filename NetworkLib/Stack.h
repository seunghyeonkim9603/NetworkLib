#pragma once

template<typename T>
class Stack final
{
public:
	Stack(unsigned int capacity);
	~Stack();

	T Pop();
	T Peek() const;
	bool TryPush(T data);

	bool IsEmpty() const;
	size_t GetSize() const;
	size_t GetCapacity() const;

private:
	T* mStack;
	size_t mTop;
	size_t mCapacity;
};

template<typename T>
inline Stack<T>::Stack(unsigned int capacity)
	:	mStack(new T[capacity]),
		mTop(0),
		mCapacity(capacity)
{
}

template<typename T>
inline Stack<T>::~Stack()
{
	delete mStack;
}

template<typename T>
inline T Stack<T>::Pop()
{
	T data = Peek();
	--mTop;
	return data;
}

template<typename T>
inline T Stack<T>::Peek() const
{
	return mStack[mTop - 1];
}

template<typename T>
inline bool Stack<T>::TryPush(T data)
{
	if (mCapacity == mTop)
	{
		return false;
	}
	mStack[mTop++] = data;
	return true;
}

template<typename T>
inline bool Stack<T>::IsEmpty() const
{
	if (mTop == 0)
	{
		return true;
	}
	return false;
}

template<typename T>
inline size_t Stack<T>::GetSize() const
{
	return mTop;
}

template<typename T>
inline size_t Stack<T>::GetCapacity() const
{
	return mCapacity;
}
