#pragma once

namespace stdext
{
	template<class T>
	void destroy_at(T* data)
	{
		data->~T();
	}
	template<class ForwardIterator>
	void destroy(ForwardIterator begin, ForwardIterator end)
	{
		typedef typename std::iterator_traits<ForwardIterator>::value_type _T;
		while (begin != end)
		{
			begin->~_T();
			++begin;
		}
	}


	template<class InputIt, class FwdIt>
	FwdIt uninitialized_move(InputIt SrcBegin, InputIt SrcEnd, FwdIt Dst)
	{
		FwdIt current = Dst;
		try
		{
			while (SrcBegin != SrcEnd)
			{
				::new (static_cast<void*>(std::addressof(*current))) typename std::iterator_traits<FwdIt>::value_type(std::move(*SrcBegin));
				++current;
				++SrcBegin;
			}
			return current;
		}
		catch (...)
		{
			destroy(Dst, current);
			throw;
		}

	}

	template<class FwdIt>
	FwdIt uninitialized_value_construct(FwdIt first, FwdIt last)
	{
		FwdIt current = first;
		try
		{
			while (current != last)
			{
				::new (static_cast<void*>(std::addressof(*current))) typename std::iterator_traits<FwdIt>::value_type();
				++current;
			}
			return current;
		}
		catch (...)
		{
			destroy(first, current);
			throw;
		}

	}
	
	template<class FwdIt>
	FwdIt uninitialized_default_construct(FwdIt first, FwdIt last)
	{
		FwdIt current = first;
		try
		{
			while (current != last)
			{
				::new (static_cast<void*>(std::addressof(*current))) typename std::iterator_traits<FwdIt>::value_type;
				++current;
			}
			return current;
		}
		catch (...)
		{
			destroy(first, current);
			throw;
		}
	}

	template<class BidirIt, class UnaryPredicate>
	BidirIt unstable_remove_if(BidirIt first, BidirIt last, UnaryPredicate p)
	{
		for (; ; ++first)
		{
			for (; first != last && !p(*first); ++first);
			if (first == last)
				break;

			for (; first != --last && p(*last); );
			if (first == last)
				break;

			*first = std::move(*last);
		}

		return (first);
	}

	//this exists as a point of reference for providing a stable comparison vs unstable_remove_if
	template<class BidirIt, class UnaryPredicate>
	BidirIt partition(BidirIt first, BidirIt last, UnaryPredicate p)
	{
		using namespace std;
		for (; ; ++first)
		{
			for (; first != last && p(*first); ++first);
			if (first == last)
				break;

			for (; first != --last && !p(*last); );
			if (first == last)
				break;

			iter_swap(first, last);
		}
		return (first);
	}

	//this exists as a point of reference for providing a stable comparison vs unstable_remove_if
	template<class ForwardIt, class UnaryPredicate>
	auto remove_if(ForwardIt first, ForwardIt last, UnaryPredicate p)
	{
		using namespace std;
		first = find_if(first, last, p);
		if (first != last)
			for (auto i = first; ++i != last; )
				if (!p(*i))
					*first++ = move(*i);
		return first;
	}
	template<class FwdIt, class Pred>
	auto semistable_partition(FwdIt first, FwdIt last, Pred p)
	{
		using namespace std;
		first = find_if(first, last, [&](auto& a) {return !p(a); });
		if (first != last)
			for (auto i = first; ++i != last; )
				if (p(*i))
					swap(*first++, *i);
		return first;
	}
}