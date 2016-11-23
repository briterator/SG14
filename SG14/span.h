#pragma once
template<class T>
struct span
{
	T* first_;
	size_t num_;
	span() = default;
	span(T* first, size_t num)
		:first_(first), num_(num)
	{}
	span(T* first, T* last)
		:span(first, last - first)
	{}
	bool operator==(const span& other)
	{
		return num_ == other.num_ && std::equal_range(begin(), end(), other.begin());
	}
	bool operator!=(const span& other)
	{
		return !(*this == other);
	}
	T* begin() { return first_; }
	constexpr const T* begin() const { return first_; }
	T* end() { return first_ + num_; }
	constexpr const T* end() const { return first_ + num_; }
	size_t size() { return num_; }
	T& operator[](size_t pos) { assert(pos < num_); return first_[pos]; }
	constexpr const T& operator[](size_t pos) const { assert(pos < num_); return first_[pos]; }
	void pop_front() { ++first_; }
	void pop_back() { num_--; }
	void pop_front(size_t n) { assert(n <= num_); first += n; }
	void pop_back(size_t n) { assert(n <= num_); num_ -= n; }
	void clear() { num_ = 0; }
	constexpr bool valid_index(size_t index) const { return index >= 0 && index < num_; }
	void slice(size_t pos, size_t n) { assert(pos + n < num_); first_ += pos; num_ = n; }
	bool overlaps(span<const T> other) { return (begin() >= other.begin() && begin() < other.end()) || (end() > other.begin() && end() <= other.end()); }
};