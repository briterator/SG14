#pragma once
#include <utility>
#include <type_traits>
#include <stdint.h>
#include <cstddef>

namespace detail_
{
	template<class T>	struct softctrl;
}
template<class T> class soft_ptr;

class enable_soft_from_this
{
	template<class T> friend soft_ptr<T> soft_from(T*);
	template<class T> friend struct detail_::softctrl;
	bool made_exposed_ = false;
};

namespace detail_
{
	template<class T>
	struct softctrl
	{
		uint32_t soft_count_ : 31;
		bool valid_ : 1;
		std::aligned_union_t<0, T> value_;
		template<class U>
		void mark_exposed(std::enable_if_t<!std::is_base_of<enable_soft_from_this, U>::value, void*> = nullptr) {}
		
		template<class U>
		void mark_exposed(std::enable_if_t<std::is_base_of<enable_soft_from_this, U>::value, void*>  = nullptr)
		{
			auto* obj = (T*)&value_;
			obj->made_exposed_ = true;
		}
	};
}
template<class T> class exposed_ptr;

//soft_ptr: single threaded weak_ptr that works with exposed_ptr
template<class T>
class soft_ptr
{
	detail_::softctrl<T>* ptr_ = nullptr;
	template<class T> friend class exposed_ptr;
	template<class T> friend class soft_ptr;
public:
	template<class U>
	soft_ptr(const exposed_ptr<U>& a)
	{
		static_assert(std::is_same<T,U>::value || std::is_base_of_v<T, U>, "Invalid cast");
		ptr_ = reinterpret_cast<decltype(ptr_)>(a.ptr_);
		if (ptr_) ptr_->soft_count_++;
	}
	template<class U>
	soft_ptr(const soft_ptr<U>& a)
	{
		static_assert(std::is_same<T, U>::value || std::is_base_of_v<T, U>, "Invalid cast");
		ptr_ = reinterpret_cast<decltype(ptr_)>(a.ptr_);
		if (ptr_) ptr_->soft_count_++;
	}

	soft_ptr() = default;
	soft_ptr(detail_::softctrl<T>* ctrl)
		:ptr_(ctrl)
	{
		if (ptr_) ptr_->soft_count_++;
	}
	soft_ptr(const soft_ptr& a)
	{
		ptr_ = a.ptr_;
		if (ptr_) ptr_->soft_count_++;
	}
	soft_ptr(soft_ptr&& a)
	{
		ptr_ = a.ptr_;
		a.ptr_ = nullptr;
	}
	T* get() const
	{
		return (ptr_ && ptr_->valid_) ? (T*)&ptr_->value_ : nullptr;
	}
	T& operator*() const
	{
		return *get();
	}
	T* operator->() const
	{
		return get();
	}
	soft_ptr& operator=(const soft_ptr& a)
	{
		clear();
		ptr_ = a.ptr_;
		if (ptr_) ptr_->soft_count_++;
		return *this;
	}
	soft_ptr& operator=(soft_ptr&& a)
	{
		clear();
		ptr_ = a.ptr_;
		a.ptr_ = nullptr;
		return *this;
	}
	soft_ptr& operator=(nullptr_t)
	{
		clear();
		return *this;
	}
	explicit operator bool() const { return ptr_ && ptr_->valid_; }
	bool operator!() const { return !static_cast<bool>(*this); }
	auto soft_count() const
	{
		return ptr_ ? ptr_->soft_count_ : 0;
	}
	void clear()
	{
		if (!ptr_) return;

		if (!ptr_->valid_ && ptr_->soft_count_ == 1)
		{
			delete ptr_;
		}
		else
		{
			ptr_->soft_count_--;
		}
		ptr_ = nullptr;
	}
	~soft_ptr()
	{
		clear();
	}
	friend bool operator==(const soft_ptr& a, nullptr_t b)
	{
		return a.get() == b;
	}
	friend bool operator!=(const soft_ptr& a, nullptr_t b)
	{
		return a.get() != b;
	}
	friend bool operator==(const soft_ptr& a, const soft_ptr& b)
	{
		return a.get() == b.get();
	}
	friend bool operator!=(const soft_ptr& a, const soft_ptr& b)
	{
		return a.get() != b.get();
	}
};



template<class T>
soft_ptr<T> soft_from(T* object)
{
	static_assert(std::is_same_v<decltype(T::made_exposed_), bool>, "must inherit from enable_soft_from_this");
	if (!object || object->made_exposed_ == false)
		return nullptr;
	constexpr int32_t offset = offsetof(detail_::softctrl<T>, value_);
	std::byte* mem = (std::byte*)object;
	mem -= offset;
	auto ctrl = (detail_::softctrl<T>*) mem;
	return { ctrl };
}


//exposed_ptr: a unique_ptr that can be weakly referenced
template<class T>
class exposed_ptr
{
	detail_::softctrl<T>* ptr_ = nullptr;
	template<class T> friend class exposed_ptr;
	template<class T> friend class soft_ptr;
public:
	exposed_ptr() = default;
	exposed_ptr(nullptr_t) {};
	exposed_ptr(const exposed_ptr&) = delete;
	exposed_ptr(exposed_ptr&& a)
	{
		ptr_ = a.ptr_;
		a.ptr_ = nullptr;
	}
	template<class U>
	exposed_ptr(exposed_ptr<U>&& a)
	{
		static_assert(std::is_base_of<T, U>::value, "Invalid type");
		ptr_ = reinterpret_cast<decltype(ptr_)>(a.ptr_);
		a.ptr_ = nullptr;
	}
	template<class... Args>
	static exposed_ptr make(Args&&... args)
	{
		exposed_ptr result;
		result.ptr_ = new detail_::softctrl<T>();
		result.ptr_->soft_count_ = 0;
		result.ptr_->valid_ = true;
		new (&result.ptr_->value_) T(std::forward<Args>(args)...);
		result.ptr_->mark_exposed<T>();
		return result;
	}
	exposed_ptr& operator=(exposed_ptr&& a)
	{
		clear();
		ptr_ = a.ptr_;
		a.ptr_ = nullptr;
		return *this;
	}
	exposed_ptr& operator=(nullptr_t)
	{
		clear();
		return *this;
	}
	T* get() const
	{
		return ptr_ ? (T*)(&ptr_->value_) : nullptr;
	}
	T& operator*() const
	{
		return *(T*)(&ptr_->value_);
	}
	T* operator->() const
	{
		return (T*)(&ptr_->value_);
	}
	
	explicit operator bool() const { return ptr_ && ptr_->valid_; }
	bool operator!() const { return !static_cast<bool>(*this); }
	auto soft_count() const
	{
		return ptr_ ? ptr_->soft_count_ : nullptr;
	}
	soft_ptr<T> soft() const
	{
		return soft_ptr<T>(*this);
	}
	void clear()
	{
		if (!ptr_) return;

		ptr_->valid_ = false;
		get()->~T();
		if (ptr_->soft_count_ == 0)
		{
			delete ptr_;
		}
		ptr_ = nullptr;
	}

	~exposed_ptr()
	{
		clear();
	}
	
	friend bool operator==(const exposed_ptr& a, nullptr_t b)
	{
		return a.get() == b;
	}
	friend bool operator!=(const exposed_ptr& a, nullptr_t b)
	{
		return a.get() != b;
	}
	friend bool operator==(const exposed_ptr& a, const exposed_ptr& b)
	{
		return a.get() == b.get();
	}
	friend bool operator!=(const exposed_ptr& a, const exposed_ptr& b)
	{
		return a.get() != b.get();
	}
};

template<class T>
bool operator==(const exposed_ptr<T>& a, const soft_ptr<T>& b)
{
	return a.get() == b.get();
}
template<class T>
bool operator==(const soft_ptr<T>& a, const exposed_ptr<T>& b)
{
	return a.get() == b.get();
}
template<class T>
bool operator!=(const exposed_ptr<T>& a, const soft_ptr<T>& b)
{
	return a.get() != b.get();
}
template<class T>
bool operator!=(const soft_ptr<T>& a, const exposed_ptr<T>& b)
{
	return a.get()!= b.get();
}