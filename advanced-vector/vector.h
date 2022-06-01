#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iterator>
#include <new>
#include <memory>
#include <type_traits>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept
        : buffer_(std::move(other.GetAddress()))
        , capacity_(std::move(other.Capacity())) {
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        buffer_ = std::move(rhs.GetAddress());
        capacity_ = std::move(rhs.Capacity());
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept
    {
        return data_ + 0;
    }

    iterator end() noexcept
    {
        return data_ + size_;
    }

    const_iterator begin() const noexcept
    {
        return data_ + 0;
    }

    const_iterator end() const noexcept
    {
        return data_ + size_;
    }

    const_iterator cbegin() const noexcept
    {
        return data_ + 0;
    }

    const_iterator cend() const noexcept
    {
        return data_ + size_;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args)
    {
        assert(pos >= begin() && pos <= end());
        if (pos == cend()) {
            EmplaceBack(std::forward<Args>(args)...);
            return data_ + size_ - 1;
        }
        size_t num_pos = std::distance(cbegin(), pos);
        if (size_ == Capacity()) {
            size_t new_capacity = size_ == 0 ? 1 : Capacity() * 2;
            RawMemory<T> new_data(new_capacity);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
            {
                new(new_data + num_pos) T(std::forward<Args>(args)...);
                std::uninitialized_move_n(data_ + 0, num_pos, new_data + 0);
                std::uninitialized_move_n(data_ + num_pos, size_ - num_pos, new_data + num_pos + 1);
            }
            else
            {
                try
                {
                    new(new_data + num_pos) T(std::forward<Args>(args)...);
                }
                catch (...) {
                    throw;
                }
                try
                {
                    std::uninitialized_copy_n(data_.GetAddress(), num_pos, new_data.GetAddress());
                }
                catch (...)
                {
                    std::destroy_n(new_data + num_pos, 1);
                    throw;
                }
                try
                {
                    std::uninitialized_copy_n(data_.GetAddress() + num_pos, size_ - num_pos, new_data.GetAddress() + num_pos + 1);
                }
                catch (...)
                {
                    std::destroy_n(new_data + 0, num_pos);
                    throw;
                }
            }
            std::destroy_n(data_.GetAddress(), size_);
            new_data.Swap(data_);
        }
        else
        {
            if (size_ != 0)
            {
                new(end()) T(std::forward<T>(data_[size_ - 1]));
                T copy(std::forward<Args>(args)...);
                std::move_backward(begin() + num_pos, end() - 1, end());
                *(data_ + num_pos) = (std::move(copy));
            }
            else
            {
                new(data_ + num_pos) T(std::forward<Args>(args)...);
            }
        }
        ++size_;
        return data_ + num_pos;
    }

    iterator Insert(const_iterator pos, const T& value)
    {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value)
    {
        return Emplace(pos, std::move(value));
    }

    iterator Erase(const_iterator pos)
    {
        assert(pos >= begin() && pos <= end());
        size_t num_pos = std::distance(cbegin(), pos);
        std::move(begin() + num_pos + 1, end(), begin() + num_pos);
        std::destroy_at(end() - 1);
        --size_;
        return data_ + num_pos;
    }

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)  //
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.Size(), data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
    {
        Swap(other);
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            DestroyN(data_ + new_size, size_ - new_size);
        }
        else if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
        }
        size_ = new_size;
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    void PopBack() {
        std::destroy_n(data_ + size_ - 1, 1);
        --size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args)
    {
        if (size_ == Capacity()) {
            size_t new_capacity = size_ == 0 ? 1 : Capacity() * 2;
            RawMemory<T> new_data(new_capacity);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
            {
                new(new_data + size_) T(std::forward<Args>(args)...);
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else
            {
                try
                {
                    new(new_data + size_) T(std::forward<Args>(args)...);
                }
                catch (...) {
                    throw;
                }
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else
        {
            new(data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return *(data_ + size_ - 1);
    }



    Vector& operator=(const Vector& rhs)
    {
        if (this != &rhs) {
            if (rhs.Size() > data_.Capacity()) {
                Vector copy(rhs);
                Swap(copy);
            }
            else
            {
                if (Size() > rhs.Size()) {
                    for (size_t i = 0; i < rhs.size_; ++i)
                    {
                        data_[i] = rhs.data_[i];
                    }
                    std::destroy_n(data_ + rhs.Size(), Size() - rhs.Size());
                }
                else {
                    for (size_t i = 0; i < size_; ++i)
                    {
                        data_[i] = rhs.data_[i];
                    }
                    std::uninitialized_copy_n(rhs.data_ + Size(), rhs.Size() - Size(), data_.GetAddress() + size_);
                }
            }
        }
        size_ = rhs.Size();
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            if (rhs.Size() > data_.Capacity()) {
                Swap(rhs);
            }
            else {
                data_.Swap(rhs.data_);
                size_ = rhs.Size();
            }
        }
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
        {
            std::uninitialized_move_n(data_.GetAddress(), Size(), new_data.GetAddress());
        }
        else
        {
            std::uninitialized_copy_n(data_.GetAddress(), size_,
                new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    ~Vector() {
        std::destroy_n(data_ + 0, size_);
    }

private:

    static void DestroyN(T* buf, size_t n) noexcept {
        std::destroy_n(buf, n);
    }

    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    static void Destroy(T* buf) noexcept {
        buf->~T();
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};