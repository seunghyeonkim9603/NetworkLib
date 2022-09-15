#pragma once

#include "ObjectPool.h"

template<typename T>
class LockFreeStack final
{
	struct Node
	{
		T Data;
		Node* Next;
	};
public:
	LockFreeStack(unsigned int capacity);
	~LockFreeStack();

	void	Push(T data);
	T		Pop();
private:
	ObjectPool<Node>	mPool;
	Node*				mTop;
	unsigned int		mCapacity;
};

template<typename T>
inline LockFreeStack<T>::LockFreeStack(unsigned int capacity)
	:	mPool(capacity),
		mTop(nullptr),
		mCapacity(capacity)
{
}

template<typename T>
inline LockFreeStack<T>::~LockFreeStack()
{
	Node* cur;
	Node* next;
	
	cur = REMOVE_OP_COUNT_FROM(mTop);
	while (cur != nullptr)
	{
		next = cur->Next;
		mPool.ReleaseObject(cur);
		cur = next;
	}
}

template<typename T>
inline void LockFreeStack<T>::Push(T data)
{
	Node* newTop = mPool.GetObject();
	Node* oldTop;

	newTop->Data = data;

	while (true)
	{
		oldTop = mTop;
		newTop->Next = REMOVE_OP_COUNT_FROM(oldTop);

		if (InterlockedCompareExchangePointer((PVOID*)&mTop, MAKE_TOP(newTop, EXTRACT_OP_COUNT_FROM(oldTop) + 1), oldTop) == oldTop)
		{
			break;
		}
	}
}

template<typename T>
inline T LockFreeStack<T>::Pop()
{
	Node* oldTop;
	Node* newTop;
	 // 성능 하락지점 있다...
	while (true)
	{
		oldTop = mTop;
		newTop = MAKE_TOP(REMOVE_OP_COUNT_FROM(oldTop)->Next, EXTRACT_OP_COUNT_FROM(oldTop) + 1);

		if (InterlockedCompareExchangePointer((PVOID*)&mTop, newTop, oldTop) == oldTop)
		{
			break;
		}
	}
	oldTop = REMOVE_OP_COUNT_FROM(oldTop);

	T data = oldTop->Data;

	mPool.ReleaseObject(oldTop);

	return data;
}