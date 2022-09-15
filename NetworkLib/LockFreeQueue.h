#pragma once

#include "ObjectPool.h"

template<typename T>
class LockFreeQueue
{
private:
	struct Node
	{
		T Data;
		Node* Next;
	};

public:
	LockFreeQueue(unsigned int capacity)
		: mPool(capacity + 1),
		mHead(nullptr),
		mTail(nullptr),
		mSize(0),
		mCapacity(capacity)
	{
		mHead = mPool.GetObject();
		mTail = mHead;
		mHead->Next = nullptr;
	}

	~LockFreeQueue()
	{
		Node* head;
		Node* next;

		head = REMOVE_OP_COUNT_FROM(mHead);

		while (head != nullptr)
		{
			next = head->Next;

			mPool.ReleaseObject(head);
			head = next;
		}
	}

	bool TryEnqueue(T data)
	{
		Node* newNode = mPool.GetObject();

		newNode->Data = data;
		newNode->Next = nullptr;

		while (true)
		{
			Node* tail = mTail;
			Node* next = REMOVE_OP_COUNT_FROM(tail)->Next;

			if (next == nullptr)
			{
				if (InterlockedCompareExchangePointer((PVOID*)&REMOVE_OP_COUNT_FROM(tail)->Next, newNode, next) == next)
				{
					InterlockedCompareExchangePointer((PVOID*)&mTail, MAKE_TOP(newNode, EXTRACT_OP_COUNT_FROM(tail) + 1), tail);
					break;
				}
			}
			else
			{
				InterlockedCompareExchangePointer((PVOID*)&mTail, MAKE_TOP(next, EXTRACT_OP_COUNT_FROM(tail) + 1), tail);
			}
		}
		InterlockedIncrement(&mSize);

		return true;
	}

	bool TryDequeue(T* outData)
	{
		if (InterlockedDecrement(&mSize) < 0)
		{
			InterlockedIncrement(&mSize);
			return false;
		}

		while (true)
		{
			Node* head = mHead;
			Node* tail = mTail;
			Node* next = REMOVE_OP_COUNT_FROM(head)->Next;

			if (next != nullptr)
			{
				if (REMOVE_OP_COUNT_FROM(head) == REMOVE_OP_COUNT_FROM(tail))
				{
					InterlockedCompareExchangePointer((PVOID*)&mTail, MAKE_TOP(next, EXTRACT_OP_COUNT_FROM(tail) + 1), tail);
				}
				*outData = next->Data;

				if (InterlockedCompareExchangePointer((PVOID*)&mHead, MAKE_TOP(next, EXTRACT_OP_COUNT_FROM(head) + 1), head) == head)
				{
					head = REMOVE_OP_COUNT_FROM(head);
					mPool.ReleaseObject(head);
					break;
				}
			}
		}
		return true;
	}

	void Clear()
	{
		Node* head;
		Node* tail;
		Node* next;

		head = REMOVE_OP_COUNT_FROM(mHead);
		tail = REMOVE_OP_COUNT_FROM(mTail);

		while (head != tail)
		{
			next = head->Next;

			mPool.ReleaseObject(head);
			head = next;
		}
		mSize = 0;
	}

	bool IsEmpty() const
	{
		if (mSize == 0)
		{
			return true;
		}
		return false;
	}

	long GetSize() const
	{
		return mSize;
	}

private:
	ObjectPool<Node> mPool;

	Node* mHead;
	Node* mTail;

	long mSize;
	long mCapacity;
};