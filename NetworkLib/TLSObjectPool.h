#pragma once

template<typename T>
class TLSObjectPool final
{
public:
	TLSObjectPool();
	~TLSObjectPool();
	TLSObjectPool(const TLSObjectPool& other) = delete;
	TLSObjectPool& operator=(const TLSObjectPool& other) = delete;

	T*		GetObject();
	void	ReleaseObject(T* obj);

private:
	static thread_local TLSObjectPoolMiddleware<T> Middleware;
};

template<typename T>
thread_local TLSObjectPoolMiddleware<T> TLSObjectPool<T>::Middleware;

template<typename T>
inline TLSObjectPool<T>::TLSObjectPool()
{
}

template<typename T>
inline TLSObjectPool<T>::~TLSObjectPool()
{
}

template<typename T>
inline T* TLSObjectPool<T>::GetObject()
{
	T* obj = Middleware.GetObject();
	return obj;
}

template<typename T>
inline void TLSObjectPool<T>::ReleaseObject(T* obj)
{
	Middleware.ReleaseObject(obj);
}
