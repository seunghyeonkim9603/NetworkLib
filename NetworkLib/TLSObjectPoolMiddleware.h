#pragma once

#include "ObjectPool.h"
#include "Chunk.h"

template<typename T>
class TLSObjectPoolMiddleware : final
{
	struct Node
	{
		Chunk<T> ObjectChunk;
		Node* Next;
	};
public:
	TLSObjectPoolMiddleware();
	~TLSObjectPoolMiddleware();
	TLSObjectPoolMiddleware(const TLSObjectPoolMiddleware& other) = delete;
	TLSObjectPoolMiddleware& operator=(const TLSObjectPoolMiddleware& other) = delete;

	T*			GetObject();
	void		ReleaseObject(T* obj);

private:
	static ObjectPool<Node> mChunkPool;
	Node*						mHead;
	Node*						mTail;
};

template<typename T>
inline TLSObjectPoolMiddleware<T>::TLSObjectPoolMiddleware()
	:	mHead(nullptr),
		mTail(nullptr)
{
	mHead = mChunkPool.GetObject();
	mTail = mHead;
}

template<typename T>
inline TLSObjectPoolMiddleware<T>::~TLSObjectPoolMiddleware()
{
	Chunk* cur;
	Chunk* next;

	cur = mHead;
	while (cur != nullptr)
	{
		next = cur->GetNext();
		mChunkPool.ReleaseObject(cur);
		cur = next;
	}
}

template<typename T>
inline T* TLSObjectPoolMiddleware<T>::GetObject()
{
	if (mTop->IsEmpty())
	{
		Chunk<T>* newTop = mChunkPool.GetObject();

		newTop->SetNext(mTop);
		mTop = newTop;
	}
	return mTop->Pop();
}

template<typename T>
inline void TLSObjectPoolMiddleware<T>::ReleaseObject(T* obj)
{
	if (mTop->IsFull())
	{

	}
}

