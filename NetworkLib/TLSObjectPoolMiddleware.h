#pragma once

template<typename T>
class TLSObjectPoolMiddleware final
{
public:
	TLSObjectPoolMiddleware();
	~TLSObjectPoolMiddleware();
	TLSObjectPoolMiddleware(const TLSObjectPoolMiddleware& other) = delete;
	TLSObjectPoolMiddleware& operator=(const TLSObjectPoolMiddleware& other) = delete;

	T*			GetObject();
	void		ReleaseObject(T* obj);

private:
	static ObjectPool<Chunk<T>> FilledChunkPool;
	static ObjectPool<Chunk<T>> EmptyChunkPool;
	Chunk<T>* mChunk;
};

template<typename T>
ObjectPool<Chunk<T>> TLSObjectPoolMiddleware<T>::FilledChunkPool;

template<typename T>
ObjectPool<Chunk<T>> TLSObjectPoolMiddleware<T>::EmptyChunkPool;

template<typename T>
inline TLSObjectPoolMiddleware<T>::TLSObjectPoolMiddleware()
	:	mChunk(nullptr)
{
	mChunk = EmptyChunkPool.GetObject();

	mChunk->Fill();
}

template<typename T>
inline TLSObjectPoolMiddleware<T>::~TLSObjectPoolMiddleware()
{
	FilledChunkPool.ReleaseObject(mChunk);
}

template<typename T>
inline T* TLSObjectPoolMiddleware<T>::GetObject()
{
	if (mChunk->IsEmpty())
	{
		EmptyChunkPool.ReleaseObject(mChunk);
		mChunk = FilledChunkPool.GetObject();

		mChunk->Fill();
	}
	return mChunk->Pop();
}

template<typename T>
inline void TLSObjectPoolMiddleware<T>::ReleaseObject(T* obj)
{
	if (mChunk->IsFull())
	{
		FilledChunkPool.ReleaseObject(mChunk);
		mChunk = EmptyChunkPool.GetObject();
	}
	mChunk->Push(obj);
}

