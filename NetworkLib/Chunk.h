#pragma once

template<typename T>
class Chunk : final
{
public:
	Chunk();
	~Chunk();

	T*		Pop();
	void	Push(const T* value);
	bool	IsFull() const;
	bool	IsEmpty() const;

private:
	enum { CHUNK_SIZE = 256 };

	const T*		mValues[CHUNK_SIZE];
	unsigned int	mTop;;
};

template<typename T>
inline Chunk<T>::Chunk()
	:	mTop(CHUNK_SIZE)
{
	for (unsigned int i = 0; i < CHUNK_SIZE; ++i)
	{
		mValues[i] = new T();
	}
}

template<typename T>
inline Chunk<T>::~Chunk()
{
	for (unsigned int i = 0; i < mTop; ++i)
	{
		delete mValues[i];
	}
}

template<typename T>
inline T* Chunk<T>::Pop()
{
	return mValues[--mTop];
}

template<typename T>
inline void Chunk<T>::Push(const T* value)
{
	mValues[mTop++] = value;
}

template<typename T>
inline bool Chunk<T>::IsFull() const
{
	if (mTop == CHUNK_SIZE)
	{
		return true;
	}
	return false;
}

template<typename T>
inline bool Chunk<T>::IsEmpty() const
{
	if (mTop == 0)
	{
		return true;
	}
	return false;
}
