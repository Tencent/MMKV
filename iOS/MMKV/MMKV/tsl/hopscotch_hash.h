/**
 * MIT License
 * 
 * Copyright (c) 2017 Tessil
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef TSL_HOPSCOTCH_HASH_H
#define TSL_HOPSCOTCH_HASH_H

#include "hopscotch_growth_policy.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#if (defined(__GNUC__) && (__GNUC__ == 4) && (__GNUC_MINOR__ < 9))
#define TSL_NO_RANGE_ERASE_WITH_CONST_ITERATOR
#endif

/*
 * Only activate tsl_assert if TSL_DEBUG is defined. 
 * This way we avoid the performance hit when NDEBUG is not defined with assert as tsl_assert is used a lot
 * (people usually compile with "-O3" and not "-O3 -DNDEBUG").
 */
#ifndef tsl_assert
#ifdef TSL_DEBUG
#define tsl_assert(expr) assert(expr)
#else
#define tsl_assert(expr) (static_cast<void>(0))
#endif
#endif

namespace tsl {

namespace detail_hopscotch_hash {

template <typename T>
struct make_void {
    using type = void;
};

template <typename T, typename = void>
struct has_is_transparent : std::false_type {};

template <typename T>
struct has_is_transparent<T, typename make_void<typename T::is_transparent>::type>
    : std::true_type {};

template <typename T, typename = void>
struct has_key_compare : std::false_type {};

template <typename T>
struct has_key_compare<T, typename make_void<typename T::key_compare>::type> : std::true_type {};

template <typename U>
struct is_power_of_two_policy : std::false_type {};

template <std::size_t GrowthFactor>
struct is_power_of_two_policy<tsl::hh::power_of_two_growth_policy<GrowthFactor>> : std::true_type {
};

/*
 * smallest_type_for_min_bits::type returns the smallest type that can fit MinBits.
 */
static const std::size_t SMALLEST_TYPE_MAX_BITS_SUPPORTED = 64;
template <unsigned int MinBits, typename Enable = void>
class smallest_type_for_min_bits {};

template <unsigned int MinBits>
class smallest_type_for_min_bits<MinBits,
                                 typename std::enable_if<(MinBits > 0) && (MinBits <= 8)>::type> {
public:
    using type = std::uint_least8_t;
};

template <unsigned int MinBits>
class smallest_type_for_min_bits<MinBits,
                                 typename std::enable_if<(MinBits > 8) && (MinBits <= 16)>::type> {
public:
    using type = std::uint_least16_t;
};

template <unsigned int MinBits>
class smallest_type_for_min_bits<MinBits,
                                 typename std::enable_if<(MinBits > 16) && (MinBits <= 32)>::type> {
public:
    using type = std::uint_least32_t;
};

template <unsigned int MinBits>
class smallest_type_for_min_bits<MinBits,
                                 typename std::enable_if<(MinBits > 32) && (MinBits <= 64)>::type> {
public:
    using type = std::uint_least64_t;
};

/*
 * Each bucket may store up to three elements:
 * - An aligned storage to store a value_type object with placement-new.
 * - An (optional) hash of the value in the bucket.
 * - An unsigned integer of type neighborhood_bitmap used to tell us which buckets in the neighborhood of the 
 *   current bucket contain a value with a hash belonging to the current bucket. 
 * 
 * For a bucket 'bct', a bit 'i' (counting from 0 and from the least significant bit to the most significant)
 * set to 1 means that the bucket 'bct + i' contains a value with a hash belonging to bucket 'bct'.
 * The bits used for that, start from the third least significant bit.
 * The two least significant bits are reserved:
 * - The least significant bit is set to 1 if there is a value in the bucket storage.
 * - The second least significant bit is set to 1 if there is an overflow. More than NeighborhoodSize values
  * give the same hash, all overflow values are stored in the m_overflow_elements list of the map.
 *
 * Details regarding hopscotch hashing an its implementation can be found here:
 *  https://tessil.github.io/2016/08/29/hopscotch-hashing.html
 */
static const std::size_t NB_RESERVED_BITS_IN_NEIGHBORHOOD = 2;

using truncated_hash_type = std::uint_least32_t;

/**
 * Helper class that store a truncated hash if StoreHash is true and nothing otherwise.
 */
template <bool StoreHash>
class hopscotch_bucket_hash {
public:
    bool bucket_hash_equal(std::size_t /*hash*/) const noexcept { return true; }

    truncated_hash_type truncated_bucket_hash() const noexcept { return 0; }

protected:
    void copy_hash(const hopscotch_bucket_hash &) noexcept {}

    void set_hash(truncated_hash_type /*hash*/) noexcept {}
};

template <>
class hopscotch_bucket_hash<true> {
public:
    bool bucket_hash_equal(std::size_t hash) const noexcept {
        return m_hash == truncated_hash_type(hash);
    }

    truncated_hash_type truncated_bucket_hash() const noexcept { return m_hash; }

protected:
    void copy_hash(const hopscotch_bucket_hash &bucket) noexcept { m_hash = bucket.m_hash; }

    void set_hash(truncated_hash_type hash) noexcept { m_hash = hash; }

private:
    truncated_hash_type m_hash;
};

template <typename ValueType, unsigned int NeighborhoodSize, bool StoreHash>
class hopscotch_bucket : public hopscotch_bucket_hash<StoreHash> {
private:
    static const std::size_t MIN_NEIGHBORHOOD_SIZE = 4;
    static const std::size_t MAX_NEIGHBORHOOD_SIZE =
        SMALLEST_TYPE_MAX_BITS_SUPPORTED - NB_RESERVED_BITS_IN_NEIGHBORHOOD;

    static_assert(NeighborhoodSize >= 4, "NeighborhoodSize should be >= 4.");
    // We can't put a variable in the message, ensure coherence
    static_assert(MIN_NEIGHBORHOOD_SIZE == 4, "");

    static_assert(NeighborhoodSize <= 62, "NeighborhoodSize should be <= 62.");
    // We can't put a variable in the message, ensure coherence
    static_assert(MAX_NEIGHBORHOOD_SIZE == 62, "");

    static_assert(!StoreHash || NeighborhoodSize <= 30,
                  "NeighborhoodSize should be <= 30 if StoreHash is true.");
    // We can't put a variable in the message, ensure coherence
    static_assert(MAX_NEIGHBORHOOD_SIZE - 32 == 30, "");

    using bucket_hash = hopscotch_bucket_hash<StoreHash>;

public:
    using value_type = ValueType;
    using neighborhood_bitmap =
        typename smallest_type_for_min_bits<NeighborhoodSize +
                                            NB_RESERVED_BITS_IN_NEIGHBORHOOD>::type;

    hopscotch_bucket() noexcept : bucket_hash(), m_neighborhood_infos(0) { tsl_assert(empty()); }

    hopscotch_bucket(const hopscotch_bucket &bucket) noexcept(
        std::is_nothrow_copy_constructible<value_type>::value)
        : bucket_hash(bucket), m_neighborhood_infos(0) {
        if (!bucket.empty()) {
            ::new (static_cast<void *>(std::addressof(m_value))) value_type(bucket.value());
        }

        m_neighborhood_infos = bucket.m_neighborhood_infos;
    }

    hopscotch_bucket(hopscotch_bucket &&bucket) noexcept(
        std::is_nothrow_move_constructible<value_type>::value)
        : bucket_hash(std::move(bucket)), m_neighborhood_infos(0) {
        if (!bucket.empty()) {
            ::new (static_cast<void *>(std::addressof(m_value)))
                value_type(std::move(bucket.value()));
        }

        m_neighborhood_infos = bucket.m_neighborhood_infos;
    }

    hopscotch_bucket &operator=(const hopscotch_bucket &bucket) noexcept(
        std::is_nothrow_copy_constructible<value_type>::value) {
        if (this != &bucket) {
            remove_value();

            bucket_hash::operator=(bucket);
            if (!bucket.empty()) {
                ::new (static_cast<void *>(std::addressof(m_value))) value_type(bucket.value());
            }

            m_neighborhood_infos = bucket.m_neighborhood_infos;
        }

        return *this;
    }

    hopscotch_bucket &operator=(hopscotch_bucket &&) = delete;

    ~hopscotch_bucket() noexcept {
        if (!empty()) {
            destroy_value();
        }
    }

    neighborhood_bitmap neighborhood_infos() const noexcept {
        return neighborhood_bitmap(m_neighborhood_infos >> NB_RESERVED_BITS_IN_NEIGHBORHOOD);
    }

    void set_overflow(bool has_overflow) noexcept {
        if (has_overflow) {
            m_neighborhood_infos = neighborhood_bitmap(m_neighborhood_infos | 2);
        } else {
            m_neighborhood_infos = neighborhood_bitmap(m_neighborhood_infos & ~2);
        }
    }

    bool has_overflow() const noexcept { return (m_neighborhood_infos & 2) != 0; }

    bool empty() const noexcept { return (m_neighborhood_infos & 1) == 0; }

    void toggle_neighbor_presence(std::size_t ineighbor) noexcept {
        tsl_assert(ineighbor <= NeighborhoodSize);
        m_neighborhood_infos = neighborhood_bitmap(
            m_neighborhood_infos ^ (1ull << (ineighbor + NB_RESERVED_BITS_IN_NEIGHBORHOOD)));
    }

    bool check_neighbor_presence(std::size_t ineighbor) const noexcept {
        tsl_assert(ineighbor <= NeighborhoodSize);
        if (((m_neighborhood_infos >> (ineighbor + NB_RESERVED_BITS_IN_NEIGHBORHOOD)) & 1) == 1) {
            return true;
        }

        return false;
    }

    value_type &value() noexcept {
        tsl_assert(!empty());
        return *reinterpret_cast<value_type *>(std::addressof(m_value));
    }

    const value_type &value() const noexcept {
        tsl_assert(!empty());
        return *reinterpret_cast<const value_type *>(std::addressof(m_value));
    }

    template <typename... Args>
    void set_value_of_empty_bucket(truncated_hash_type hash, Args &&... value_type_args) {
        tsl_assert(empty());

        ::new (static_cast<void *>(std::addressof(m_value)))
            value_type(std::forward<Args>(value_type_args)...);
        set_empty(false);
        this->set_hash(hash);
    }

    void swap_value_into_empty_bucket(hopscotch_bucket &empty_bucket) {
        tsl_assert(empty_bucket.empty());
        if (!empty()) {
            ::new (static_cast<void *>(std::addressof(empty_bucket.m_value)))
                value_type(std::move(value()));
            empty_bucket.copy_hash(*this);
            empty_bucket.set_empty(false);

            destroy_value();
            set_empty(true);
        }
    }

    void remove_value() noexcept {
        if (!empty()) {
            destroy_value();
            set_empty(true);
        }
    }

    void clear() noexcept {
        if (!empty()) {
            destroy_value();
        }

        m_neighborhood_infos = 0;
        tsl_assert(empty());
    }

    static std::size_t max_size() noexcept {
        if (StoreHash) {
            return std::numeric_limits<typename bucket_hash::hash_type>::max();
        } else {
            return std::numeric_limits<std::size_t>::max();
        }
    }

    static truncated_hash_type truncate_hash(std::size_t hash) noexcept {
        return truncated_hash_type(hash);
    }

private:
    void set_empty(bool is_empty) noexcept {
        if (is_empty) {
            m_neighborhood_infos = neighborhood_bitmap(m_neighborhood_infos & ~1);
        } else {
            m_neighborhood_infos = neighborhood_bitmap(m_neighborhood_infos | 1);
        }
    }

    void destroy_value() noexcept {
        tsl_assert(!empty());
        value().~value_type();
    }

private:
    using storage = typename std::aligned_storage<sizeof(value_type), alignof(value_type)>::type;

    neighborhood_bitmap m_neighborhood_infos;
    storage m_value;
};

/**
 * Internal common class used by (b)hopscotch_map and (b)hopscotch_set.
 * 
 * ValueType is what will be stored by hopscotch_hash (usually std::pair<Key, T> for a map and Key for a set).
 * 
 * KeySelect should be a FunctionObject which takes a ValueType in parameter and returns a reference to the key.
 * 
 * ValueSelect should be a FunctionObject which takes a ValueType in parameter and returns a reference to the value.
 * ValueSelect should be void if there is no value (in a set for example).
 * 
 * OverflowContainer will be used as containers for overflown elements. Usually it should be a list<ValueType>
 * or a set<Key>/map<Key, T>.
 */
template <class ValueType,
          class KeySelect,
          class ValueSelect,
          class Hash,
          class KeyEqual,
          class Allocator,
          unsigned int NeighborhoodSize,
          bool StoreHash,
          class GrowthPolicy,
          class OverflowContainer>
class hopscotch_hash : private Hash, private KeyEqual, private GrowthPolicy {
private:
    template <typename U>
    using has_mapped_type = typename std::integral_constant<bool, !std::is_same<U, void>::value>;

    static_assert(noexcept(std::declval<GrowthPolicy>().bucket_for_hash(std::size_t(0))),
                  "GrowthPolicy::bucket_for_hash must be noexcept.");
    static_assert(noexcept(std::declval<GrowthPolicy>().clear()),
                  "GrowthPolicy::clear must be noexcept.");

public:
    template <bool IsConst>
    class hopscotch_iterator;

    using key_type = typename KeySelect::key_type;
    using value_type = ValueType;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = Allocator;
    using reference = value_type &;
    using const_reference = const value_type &;
    using pointer = value_type *;
    using const_pointer = const value_type *;
    using iterator = hopscotch_iterator<false>;
    using const_iterator = hopscotch_iterator<true>;

private:
    using hopscotch_bucket =
        tsl::detail_hopscotch_hash::hopscotch_bucket<ValueType, NeighborhoodSize, StoreHash>;
    using neighborhood_bitmap = typename hopscotch_bucket::neighborhood_bitmap;

    using buckets_allocator =
        typename std::allocator_traits<allocator_type>::template rebind_alloc<hopscotch_bucket>;
    using buckets_container_type = std::vector<hopscotch_bucket, buckets_allocator>;

    using overflow_container_type = OverflowContainer;

    static_assert(std::is_same<typename overflow_container_type::value_type, ValueType>::value,
                  "OverflowContainer should have ValueType as type.");

    static_assert(std::is_same<typename overflow_container_type::allocator_type, Allocator>::value,
                  "Invalid allocator, not the same type as the value_type.");

    using iterator_buckets = typename buckets_container_type::iterator;
    using const_iterator_buckets = typename buckets_container_type::const_iterator;

    using iterator_overflow = typename overflow_container_type::iterator;
    using const_iterator_overflow = typename overflow_container_type::const_iterator;

public:
    /**
     * The `operator*()` and `operator->()` methods return a const reference and const pointer respectively to the 
     * stored value type.
     * 
     * In case of a map, to get a modifiable reference to the value associated to a key (the `.second` in the 
     * stored pair), you have to call `value()`.
     */
    template <bool IsConst>
    class hopscotch_iterator {
        friend class hopscotch_hash;

    private:
        using iterator_bucket =
            typename std::conditional<IsConst,
                                      typename hopscotch_hash::const_iterator_buckets,
                                      typename hopscotch_hash::iterator_buckets>::type;
        using iterator_overflow =
            typename std::conditional<IsConst,
                                      typename hopscotch_hash::const_iterator_overflow,
                                      typename hopscotch_hash::iterator_overflow>::type;

        hopscotch_iterator(iterator_bucket buckets_iterator,
                           iterator_bucket buckets_end_iterator,
                           iterator_overflow overflow_iterator) noexcept
            : m_buckets_iterator(buckets_iterator)
            , m_buckets_end_iterator(buckets_end_iterator)
            , m_overflow_iterator(overflow_iterator) {}

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = const typename hopscotch_hash::value_type;
        using difference_type = std::ptrdiff_t;
        using reference = value_type &;
        using pointer = value_type *;

        hopscotch_iterator() noexcept {}

        hopscotch_iterator(const hopscotch_iterator<false> &other) noexcept
            : m_buckets_iterator(other.m_buckets_iterator)
            , m_buckets_end_iterator(other.m_buckets_end_iterator)
            , m_overflow_iterator(other.m_overflow_iterator) {}

        const typename hopscotch_hash::key_type &key() const {
            if (m_buckets_iterator != m_buckets_end_iterator) {
                return KeySelect()(m_buckets_iterator->value());
            }

            return KeySelect()(*m_overflow_iterator);
        }

        template <class U = ValueSelect,
                  typename std::enable_if<has_mapped_type<U>::value>::type * = nullptr>
        typename std::
            conditional<IsConst, const typename U::value_type &, typename U::value_type &>::type
            value() const {
            if (m_buckets_iterator != m_buckets_end_iterator) {
                return U()(m_buckets_iterator->value());
            }

            return U()(*m_overflow_iterator);
        }

        reference operator*() const {
            if (m_buckets_iterator != m_buckets_end_iterator) {
                return m_buckets_iterator->value();
            }

            return *m_overflow_iterator;
        }

        pointer operator->() const {
            if (m_buckets_iterator != m_buckets_end_iterator) {
                return std::addressof(m_buckets_iterator->value());
            }

            return std::addressof(*m_overflow_iterator);
        }

        hopscotch_iterator &operator++() {
            if (m_buckets_iterator == m_buckets_end_iterator) {
                ++m_overflow_iterator;
                return *this;
            }

            do {
                ++m_buckets_iterator;
            } while (m_buckets_iterator != m_buckets_end_iterator && m_buckets_iterator->empty());

            return *this;
        }

        hopscotch_iterator operator++(int) {
            hopscotch_iterator tmp(*this);
            ++*this;

            return tmp;
        }

        friend bool operator==(const hopscotch_iterator &lhs, const hopscotch_iterator &rhs) {
            return lhs.m_buckets_iterator == rhs.m_buckets_iterator &&
                   lhs.m_overflow_iterator == rhs.m_overflow_iterator;
        }

        friend bool operator!=(const hopscotch_iterator &lhs, const hopscotch_iterator &rhs) {
            return !(lhs == rhs);
        }

    private:
        iterator_bucket m_buckets_iterator;
        iterator_bucket m_buckets_end_iterator;
        iterator_overflow m_overflow_iterator;
    };

public:
    template <class OC = OverflowContainer,
              typename std::enable_if<!has_key_compare<OC>::value>::type * = nullptr>
    hopscotch_hash(size_type bucket_count,
                   const Hash &hash,
                   const KeyEqual &equal,
                   const Allocator &alloc,
                   float max_load_factor)
        : Hash(hash)
        , KeyEqual(equal)
        , GrowthPolicy(bucket_count)
        , m_buckets(alloc)
        , m_overflow_elements(alloc)
        , m_first_or_empty_bucket(static_empty_bucket_ptr())
        , m_nb_elements(0) {
        if (bucket_count > max_bucket_count()) {
            throw std::length_error("The map exceeds its maxmimum size.");
        }

        if (bucket_count > 0) {
            static_assert(NeighborhoodSize - 1 > 0, "");

            // Can't directly construct with the appropriate size in the initializer
            // as m_buckets(bucket_count, alloc) is not supported by GCC 4.8
            m_buckets.resize(bucket_count + NeighborhoodSize - 1);
            m_first_or_empty_bucket = m_buckets.data();
        }

        this->max_load_factor(max_load_factor);

        // Check in the constructor instead of outside of a function to avoi compilation issues
        // when value_type is not complete.
        static_assert(
            std::is_nothrow_move_constructible<value_type>::value ||
                std::is_copy_constructible<value_type>::value,
            "value_type must be either copy constructible or nothrow move constructible.");
    }

    template <class OC = OverflowContainer,
              typename std::enable_if<has_key_compare<OC>::value>::type * = nullptr>
    hopscotch_hash(size_type bucket_count,
                   const Hash &hash,
                   const KeyEqual &equal,
                   const Allocator &alloc,
                   float max_load_factor,
                   const typename OC::key_compare &comp)
        : Hash(hash)
        , KeyEqual(equal)
        , GrowthPolicy(bucket_count)
        , m_buckets(alloc)
        , m_overflow_elements(comp, alloc)
        , m_first_or_empty_bucket(static_empty_bucket_ptr())
        , m_nb_elements(0) {

        if (bucket_count > max_bucket_count()) {
            throw std::length_error("The map exceeds its maxmimum size.");
        }

        if (bucket_count > 0) {
            static_assert(NeighborhoodSize - 1 > 0, "");

            // Can't directly construct with the appropriate size in the initializer
            // as m_buckets(bucket_count, alloc) is not supported by GCC 4.8
            m_buckets.resize(bucket_count + NeighborhoodSize - 1);
            m_first_or_empty_bucket = m_buckets.data();
        }

        this->max_load_factor(max_load_factor);

        // Check in the constructor instead of outside of a function to avoi compilation issues
        // when value_type is not complete.
        static_assert(
            std::is_nothrow_move_constructible<value_type>::value ||
                std::is_copy_constructible<value_type>::value,
            "value_type must be either copy constructible or nothrow move constructible.");
    }

    hopscotch_hash(const hopscotch_hash &other)
        : Hash(other)
        , KeyEqual(other)
        , GrowthPolicy(other)
        , m_buckets(other.m_buckets)
        , m_overflow_elements(other.m_overflow_elements)
        , m_first_or_empty_bucket(m_buckets.empty() ? static_empty_bucket_ptr() : m_buckets.data())
        , m_nb_elements(other.m_nb_elements)
        , m_max_load_factor(other.m_max_load_factor)
        , m_max_load_threshold_rehash(other.m_max_load_threshold_rehash)
        , m_min_load_threshold_rehash(other.m_min_load_threshold_rehash) {}

    hopscotch_hash(hopscotch_hash &&other) noexcept(
        std::is_nothrow_move_constructible<Hash>::value &&std::is_nothrow_move_constructible<
            KeyEqual>::value &&std::is_nothrow_move_constructible<GrowthPolicy>::value
            &&std::is_nothrow_move_constructible<buckets_container_type>::value
                &&std::is_nothrow_move_constructible<overflow_container_type>::value)
        : Hash(std::move(static_cast<Hash &>(other)))
        , KeyEqual(std::move(static_cast<KeyEqual &>(other)))
        , GrowthPolicy(std::move(static_cast<GrowthPolicy &>(other)))
        , m_buckets(std::move(other.m_buckets))
        , m_overflow_elements(std::move(other.m_overflow_elements))
        , m_first_or_empty_bucket(m_buckets.empty() ? static_empty_bucket_ptr() : m_buckets.data())
        , m_nb_elements(other.m_nb_elements)
        , m_max_load_factor(other.m_max_load_factor)
        , m_max_load_threshold_rehash(other.m_max_load_threshold_rehash)
        , m_min_load_threshold_rehash(other.m_min_load_threshold_rehash) {
        other.GrowthPolicy::clear();
        other.m_buckets.clear();
        other.m_overflow_elements.clear();
        other.m_first_or_empty_bucket = static_empty_bucket_ptr();
        other.m_nb_elements = 0;
        other.m_max_load_threshold_rehash = 0;
        other.m_min_load_threshold_rehash = 0;
    }

    hopscotch_hash &operator=(const hopscotch_hash &other) {
        if (&other != this) {
            Hash::operator=(other);
            KeyEqual::operator=(other);
            GrowthPolicy::operator=(other);

            m_buckets = other.m_buckets;
            m_overflow_elements = other.m_overflow_elements;
            m_first_or_empty_bucket =
                m_buckets.empty() ? static_empty_bucket_ptr() : m_buckets.data();
            m_nb_elements = other.m_nb_elements;
            m_max_load_factor = other.m_max_load_factor;
            m_max_load_threshold_rehash = other.m_max_load_threshold_rehash;
            m_min_load_threshold_rehash = other.m_min_load_threshold_rehash;
        }

        return *this;
    }

    hopscotch_hash &operator=(hopscotch_hash &&other) {
        other.swap(*this);
        other.clear();

        return *this;
    }

    allocator_type get_allocator() const { return m_buckets.get_allocator(); }

    /*
     * Iterators
     */
    iterator begin() noexcept {
        auto begin = m_buckets.begin();
        while (begin != m_buckets.end() && begin->empty()) {
            ++begin;
        }

        return iterator(begin, m_buckets.end(), m_overflow_elements.begin());
    }

    const_iterator begin() const noexcept { return cbegin(); }

    const_iterator cbegin() const noexcept {
        auto begin = m_buckets.cbegin();
        while (begin != m_buckets.cend() && begin->empty()) {
            ++begin;
        }

        return const_iterator(begin, m_buckets.cend(), m_overflow_elements.cbegin());
    }

    iterator end() noexcept {
        return iterator(m_buckets.end(), m_buckets.end(), m_overflow_elements.end());
    }

    const_iterator end() const noexcept { return cend(); }

    const_iterator cend() const noexcept {
        return const_iterator(m_buckets.cend(), m_buckets.cend(), m_overflow_elements.cend());
    }

    /*
     * Capacity
     */
    bool empty() const noexcept { return m_nb_elements == 0; }

    size_type size() const noexcept { return m_nb_elements; }

    size_type max_size() const noexcept { return hopscotch_bucket::max_size(); }

    /*
     * Modifiers
     */
    void clear() noexcept {
        for (auto &bucket : m_buckets) {
            bucket.clear();
        }

        m_overflow_elements.clear();
        m_nb_elements = 0;
    }

    std::pair<iterator, bool> insert(const value_type &value) { return insert_impl(value); }

    template <
        class P,
        typename std::enable_if<std::is_constructible<value_type, P &&>::value>::type * = nullptr>
    std::pair<iterator, bool> insert(P &&value) {
        return insert_impl(value_type(std::forward<P>(value)));
    }

    std::pair<iterator, bool> insert(value_type &&value) { return insert_impl(std::move(value)); }

    iterator insert(const_iterator hint, const value_type &value) {
        if (hint != cend() && compare_keys(KeySelect()(*hint), KeySelect()(value))) {
            return mutable_iterator(hint);
        }

        return insert(value).first;
    }

    template <
        class P,
        typename std::enable_if<std::is_constructible<value_type, P &&>::value>::type * = nullptr>
    iterator insert(const_iterator hint, P &&value) {
        return emplace_hint(hint, std::forward<P>(value));
    }

    iterator insert(const_iterator hint, value_type &&value) {
        if (hint != cend() && compare_keys(KeySelect()(*hint), KeySelect()(value))) {
            return mutable_iterator(hint);
        }

        return insert(std::move(value)).first;
    }

    template <class InputIt>
    void insert(InputIt first, InputIt last) {
        if (std::is_base_of<std::forward_iterator_tag,
                            typename std::iterator_traits<InputIt>::iterator_category>::value) {
            const auto nb_elements_insert = std::distance(first, last);
            const std::size_t nb_elements_in_buckets = m_nb_elements - m_overflow_elements.size();
            const std::size_t nb_free_buckets =
                m_max_load_threshold_rehash - nb_elements_in_buckets;
            tsl_assert(m_nb_elements >= m_overflow_elements.size());
            tsl_assert(m_max_load_threshold_rehash >= nb_elements_in_buckets);

            if (nb_elements_insert > 0 && nb_free_buckets < std::size_t(nb_elements_insert)) {
                reserve(nb_elements_in_buckets + std::size_t(nb_elements_insert));
            }
        }

        for (; first != last; ++first) {
            insert(*first);
        }
    }

    template <class M>
    std::pair<iterator, bool> insert_or_assign(const key_type &k, M &&obj) {
        return insert_or_assign_impl(k, std::forward<M>(obj));
    }

    template <class M>
    std::pair<iterator, bool> insert_or_assign(key_type &&k, M &&obj) {
        return insert_or_assign_impl(std::move(k), std::forward<M>(obj));
    }

    template <class M>
    iterator insert_or_assign(const_iterator hint, const key_type &k, M &&obj) {
        if (hint != cend() && compare_keys(KeySelect()(*hint), k)) {
            auto it = mutable_iterator(hint);
            it.value() = std::forward<M>(obj);

            return it;
        }

        return insert_or_assign(k, std::forward<M>(obj)).first;
    }

    template <class M>
    iterator insert_or_assign(const_iterator hint, key_type &&k, M &&obj) {
        if (hint != cend() && compare_keys(KeySelect()(*hint), k)) {
            auto it = mutable_iterator(hint);
            it.value() = std::forward<M>(obj);

            return it;
        }

        return insert_or_assign(std::move(k), std::forward<M>(obj)).first;
    }

    template <class... Args>
    std::pair<iterator, bool> emplace(Args &&... args) {
        return insert(value_type(std::forward<Args>(args)...));
    }

    template <class... Args>
    iterator emplace_hint(const_iterator hint, Args &&... args) {
        return insert(hint, value_type(std::forward<Args>(args)...));
    }

    template <class... Args>
    std::pair<iterator, bool> try_emplace(const key_type &k, Args &&... args) {
        return try_emplace_impl(k, std::forward<Args>(args)...);
    }

    template <class... Args>
    std::pair<iterator, bool> try_emplace(key_type &&k, Args &&... args) {
        return try_emplace_impl(std::move(k), std::forward<Args>(args)...);
    }

    template <class... Args>
    iterator try_emplace(const_iterator hint, const key_type &k, Args &&... args) {
        if (hint != cend() && compare_keys(KeySelect()(*hint), k)) {
            return mutable_iterator(hint);
        }

        return try_emplace(k, std::forward<Args>(args)...).first;
    }

    template <class... Args>
    iterator try_emplace(const_iterator hint, key_type &&k, Args &&... args) {
        if (hint != cend() && compare_keys(KeySelect()(*hint), k)) {
            return mutable_iterator(hint);
        }

        return try_emplace(std::move(k), std::forward<Args>(args)...).first;
    }

    /**
     * Here to avoid `template<class K> size_type erase(const K& key)` being used when
     * we use an iterator instead of a const_iterator.
     */
    iterator erase(iterator pos) { return erase(const_iterator(pos)); }

    iterator erase(const_iterator pos) {
        const std::size_t ibucket_for_hash = bucket_for_hash(hash_key(pos.key()));

        if (pos.m_buckets_iterator != pos.m_buckets_end_iterator) {
            auto it_bucket =
                m_buckets.begin() + std::distance(m_buckets.cbegin(), pos.m_buckets_iterator);
            erase_from_bucket(*it_bucket, ibucket_for_hash);

            return ++iterator(it_bucket, m_buckets.end(), m_overflow_elements.begin());
        } else {
            auto it_next_overflow = erase_from_overflow(pos.m_overflow_iterator, ibucket_for_hash);
            return iterator(m_buckets.end(), m_buckets.end(), it_next_overflow);
        }
    }

    iterator erase(const_iterator first, const_iterator last) {
        if (first == last) {
            return mutable_iterator(first);
        }

        auto to_delete = erase(first);
        while (to_delete != last) {
            to_delete = erase(to_delete);
        }

        return to_delete;
    }

    template <class K>
    size_type erase(const K &key) {
        return erase(key, hash_key(key));
    }

    template <class K>
    size_type erase(const K &key, std::size_t hash) {
        const std::size_t ibucket_for_hash = bucket_for_hash(hash);

        hopscotch_bucket *bucket_found =
            find_in_buckets(key, hash, m_first_or_empty_bucket + ibucket_for_hash);
        if (bucket_found != nullptr) {
            erase_from_bucket(*bucket_found, ibucket_for_hash);

            return 1;
        }

        if ((m_first_or_empty_bucket + ibucket_for_hash)->has_overflow()) {
            auto it_overflow = find_in_overflow(key);
            if (it_overflow != m_overflow_elements.end()) {
                erase_from_overflow(it_overflow, ibucket_for_hash);

                return 1;
            }
        }

        return 0;
    }

    void swap(hopscotch_hash &other) {
        using std::swap;

        swap(static_cast<Hash &>(*this), static_cast<Hash &>(other));
        swap(static_cast<KeyEqual &>(*this), static_cast<KeyEqual &>(other));
        swap(static_cast<GrowthPolicy &>(*this), static_cast<GrowthPolicy &>(other));
        swap(m_buckets, other.m_buckets);
        swap(m_overflow_elements, other.m_overflow_elements);
        swap(m_first_or_empty_bucket, other.m_first_or_empty_bucket);
        swap(m_nb_elements, other.m_nb_elements);
        swap(m_max_load_factor, other.m_max_load_factor);
        swap(m_max_load_threshold_rehash, other.m_max_load_threshold_rehash);
        swap(m_min_load_threshold_rehash, other.m_min_load_threshold_rehash);
    }

    /*
     * Lookup
     */
    template <class K,
              class U = ValueSelect,
              typename std::enable_if<has_mapped_type<U>::value>::type * = nullptr>
    typename U::value_type &at(const K &key) {
        return at(key, hash_key(key));
    }

    template <class K,
              class U = ValueSelect,
              typename std::enable_if<has_mapped_type<U>::value>::type * = nullptr>
    typename U::value_type &at(const K &key, std::size_t hash) {
        return const_cast<typename U::value_type &>(
            static_cast<const hopscotch_hash *>(this)->at(key, hash));
    }

    template <class K,
              class U = ValueSelect,
              typename std::enable_if<has_mapped_type<U>::value>::type * = nullptr>
    const typename U::value_type &at(const K &key) const {
        return at(key, hash_key(key));
    }

    template <class K,
              class U = ValueSelect,
              typename std::enable_if<has_mapped_type<U>::value>::type * = nullptr>
    const typename U::value_type &at(const K &key, std::size_t hash) const {
        using T = typename U::value_type;

        const T *value =
            find_value_impl(key, hash, m_first_or_empty_bucket + bucket_for_hash(hash));
        if (value == nullptr) {
            throw std::out_of_range("Couldn't find key.");
        } else {
            return *value;
        }
    }

    template <class K,
              class U = ValueSelect,
              typename std::enable_if<has_mapped_type<U>::value>::type * = nullptr>
    typename U::value_type &operator[](K &&key) {
        using T = typename U::value_type;

        const std::size_t hash = hash_key(key);
        const std::size_t ibucket_for_hash = bucket_for_hash(hash);

        T *value = find_value_impl(key, hash, m_first_or_empty_bucket + ibucket_for_hash);
        if (value != nullptr) {
            return *value;
        } else {
            return insert_impl(ibucket_for_hash, hash, std::piecewise_construct,
                               std::forward_as_tuple(std::forward<K>(key)), std::forward_as_tuple())
                .first.value();
        }
    }

    template <class K>
    size_type count(const K &key) const {
        return count(key, hash_key(key));
    }

    template <class K>
    size_type count(const K &key, std::size_t hash) const {
        return count_impl(key, hash, m_first_or_empty_bucket + bucket_for_hash(hash));
    }

    template <class K>
    iterator find(const K &key) {
        return find(key, hash_key(key));
    }

    template <class K>
    iterator find(const K &key, std::size_t hash) {
        return find_impl(key, hash, m_first_or_empty_bucket + bucket_for_hash(hash));
    }

    template <class K>
    const_iterator find(const K &key) const {
        return find(key, hash_key(key));
    }

    template <class K>
    const_iterator find(const K &key, std::size_t hash) const {
        return find_impl(key, hash, m_first_or_empty_bucket + bucket_for_hash(hash));
    }

    template <class K>
    std::pair<iterator, iterator> equal_range(const K &key) {
        return equal_range(key, hash_key(key));
    }

    template <class K>
    std::pair<iterator, iterator> equal_range(const K &key, std::size_t hash) {
        iterator it = find(key, hash);
        return std::make_pair(it, (it == end()) ? it : std::next(it));
    }

    template <class K>
    std::pair<const_iterator, const_iterator> equal_range(const K &key) const {
        return equal_range(key, hash_key(key));
    }

    template <class K>
    std::pair<const_iterator, const_iterator> equal_range(const K &key, std::size_t hash) const {
        const_iterator it = find(key, hash);
        return std::make_pair(it, (it == cend()) ? it : std::next(it));
    }

    /*
     * Bucket interface 
     */
    size_type bucket_count() const {
        /*
         * So that the last bucket can have NeighborhoodSize neighbors, the size of the bucket array is a little
         * bigger than the real number of buckets when not empty. 
         * We could use some of the buckets at the beginning, but it is faster this way as we avoid extra checks.
         */
        if (m_buckets.empty()) {
            return 0;
        }

        return m_buckets.size() - NeighborhoodSize + 1;
    }

    size_type max_bucket_count() const {
        const std::size_t max_bucket_count =
            std::min(GrowthPolicy::max_bucket_count(), m_buckets.max_size());
        return max_bucket_count - NeighborhoodSize + 1;
    }

    /*
     *  Hash policy 
     */
    float load_factor() const {
        if (bucket_count() == 0) {
            return 0;
        }

        return float(m_nb_elements) / float(bucket_count());
    }

    float max_load_factor() const { return m_max_load_factor; }

    void max_load_factor(float ml) {
        m_max_load_factor = std::max(0.1f, std::min(ml, 0.95f));
        m_max_load_threshold_rehash = size_type(float(bucket_count()) * m_max_load_factor);
        m_min_load_threshold_rehash = size_type(float(bucket_count()) * MIN_LOAD_FACTOR_FOR_REHASH);
    }

    void rehash(size_type count_) {
        count_ = std::max(count_, size_type(std::ceil(float(size()) / max_load_factor())));
        rehash_impl(count_);
    }

    void reserve(size_type count_) {
        rehash(size_type(std::ceil(float(count_) / max_load_factor())));
    }

    /*
     * Observers
     */
    hasher hash_function() const { return static_cast<const Hash &>(*this); }

    key_equal key_eq() const { return static_cast<const KeyEqual &>(*this); }

    /*
     * Other
     */
    iterator mutable_iterator(const_iterator pos) {
        if (pos.m_buckets_iterator != pos.m_buckets_end_iterator) {
            // Get a non-const iterator
            auto it = m_buckets.begin() + std::distance(m_buckets.cbegin(), pos.m_buckets_iterator);
            return iterator(it, m_buckets.end(), m_overflow_elements.begin());
        } else {
            // Get a non-const iterator
            auto it = mutable_overflow_iterator(pos.m_overflow_iterator);
            return iterator(m_buckets.end(), m_buckets.end(), it);
        }
    }

    size_type overflow_size() const noexcept { return m_overflow_elements.size(); }

    template <class U = OverflowContainer,
              typename std::enable_if<has_key_compare<U>::value>::type * = nullptr>
    typename U::key_compare key_comp() const {
        return m_overflow_elements.key_comp();
    }

private:
    template <class K>
    std::size_t hash_key(const K &key) const {
        return Hash::operator()(key);
    }

    template <class K1, class K2>
    bool compare_keys(const K1 &key1, const K2 &key2) const {
        return KeyEqual::operator()(key1, key2);
    }

    std::size_t bucket_for_hash(std::size_t hash) const {
        const std::size_t bucket = GrowthPolicy::bucket_for_hash(hash);
        tsl_assert(bucket < m_buckets.size() || (bucket == 0 && m_buckets.empty()));

        return bucket;
    }

    template <
        typename U = value_type,
        typename std::enable_if<std::is_nothrow_move_constructible<U>::value>::type * = nullptr>
    void rehash_impl(size_type count_) {
        hopscotch_hash new_map = new_hopscotch_hash(count_);

        if (!m_overflow_elements.empty()) {
            new_map.m_overflow_elements.swap(m_overflow_elements);
            new_map.m_nb_elements += new_map.m_overflow_elements.size();

            for (const value_type &value : new_map.m_overflow_elements) {
                const std::size_t ibucket_for_hash =
                    new_map.bucket_for_hash(new_map.hash_key(KeySelect()(value)));
                new_map.m_buckets[ibucket_for_hash].set_overflow(true);
            }
        }

        try {
            const bool use_stored_hash = USE_STORED_HASH_ON_REHASH(new_map.bucket_count());
            for (auto it_bucket = m_buckets.begin(); it_bucket != m_buckets.end(); ++it_bucket) {
                if (it_bucket->empty()) {
                    continue;
                }

                const std::size_t hash = use_stored_hash
                                             ? it_bucket->truncated_bucket_hash()
                                             : new_map.hash_key(KeySelect()(it_bucket->value()));
                const std::size_t ibucket_for_hash = new_map.bucket_for_hash(hash);

                new_map.insert_impl(ibucket_for_hash, hash, std::move(it_bucket->value()));

                erase_from_bucket(*it_bucket, bucket_for_hash(hash));
            }
        }
        /*
         * The call to insert_impl may throw an exception if an element is added to the overflow
         * list. Rollback the elements in this case.
         */
        catch (...) {
            m_overflow_elements.swap(new_map.m_overflow_elements);

            const bool use_stored_hash = USE_STORED_HASH_ON_REHASH(new_map.bucket_count());
            for (auto it_bucket = new_map.m_buckets.begin(); it_bucket != new_map.m_buckets.end();
                 ++it_bucket) {
                if (it_bucket->empty()) {
                    continue;
                }

                const std::size_t hash = use_stored_hash
                                             ? it_bucket->truncated_bucket_hash()
                                             : hash_key(KeySelect()(it_bucket->value()));
                const std::size_t ibucket_for_hash = bucket_for_hash(hash);

                // The elements we insert were not in the overflow list before the switch.
                // They will not be go in the overflow list if we rollback the switch.
                insert_impl(ibucket_for_hash, hash, std::move(it_bucket->value()));
            }

            throw;
        }

        new_map.swap(*this);
    }

    template <
        typename U = value_type,
        typename std::enable_if<std::is_copy_constructible<U>::value &&
                                !std::is_nothrow_move_constructible<U>::value>::type * = nullptr>
    void rehash_impl(size_type count_) {
        hopscotch_hash new_map = new_hopscotch_hash(count_);

        const bool use_stored_hash = USE_STORED_HASH_ON_REHASH(new_map.bucket_count());
        for (const hopscotch_bucket &bucket : m_buckets) {
            if (bucket.empty()) {
                continue;
            }

            const std::size_t hash = use_stored_hash
                                         ? bucket.truncated_bucket_hash()
                                         : new_map.hash_key(KeySelect()(bucket.value()));
            const std::size_t ibucket_for_hash = new_map.bucket_for_hash(hash);

            new_map.insert_impl(ibucket_for_hash, hash, bucket.value());
        }

        for (const value_type &value : m_overflow_elements) {
            const std::size_t hash = new_map.hash_key(KeySelect()(value));
            const std::size_t ibucket_for_hash = new_map.bucket_for_hash(hash);

            new_map.insert_impl(ibucket_for_hash, hash, value);
        }

        new_map.swap(*this);
    }

#ifdef TSL_NO_RANGE_ERASE_WITH_CONST_ITERATOR
    iterator_overflow mutable_overflow_iterator(const_iterator_overflow it) {
        return std::next(m_overflow_elements.begin(),
                         std::distance(m_overflow_elements.cbegin(), it));
    }
#else
    iterator_overflow mutable_overflow_iterator(const_iterator_overflow it) {
        return m_overflow_elements.erase(it, it);
    }
#endif

    // iterator is in overflow list
    iterator_overflow erase_from_overflow(const_iterator_overflow pos,
                                          std::size_t ibucket_for_hash) {
#ifdef TSL_NO_RANGE_ERASE_WITH_CONST_ITERATOR
        auto it_next = m_overflow_elements.erase(mutable_overflow_iterator(pos));
#else
        auto it_next = m_overflow_elements.erase(pos);
#endif
        m_nb_elements--;

        // Check if we can remove the overflow flag
        tsl_assert(m_buckets[ibucket_for_hash].has_overflow());
        for (const value_type &value : m_overflow_elements) {
            const std::size_t bucket_for_value = bucket_for_hash(hash_key(KeySelect()(value)));
            if (bucket_for_value == ibucket_for_hash) {
                return it_next;
            }
        }

        m_buckets[ibucket_for_hash].set_overflow(false);
        return it_next;
    }

    /**
     * bucket_for_value is the bucket in which the value is.
     * ibucket_for_hash is the bucket where the value belongs.
     */
    void erase_from_bucket(hopscotch_bucket &bucket_for_value,
                           std::size_t ibucket_for_hash) noexcept {
        const std::size_t ibucket_for_value = std::distance(m_buckets.data(), &bucket_for_value);
        tsl_assert(ibucket_for_value >= ibucket_for_hash);

        bucket_for_value.remove_value();
        m_buckets[ibucket_for_hash].toggle_neighbor_presence(ibucket_for_value - ibucket_for_hash);
        m_nb_elements--;
    }

    template <class K, class M>
    std::pair<iterator, bool> insert_or_assign_impl(K &&key, M &&obj) {
        auto it = try_emplace_impl(std::forward<K>(key), std::forward<M>(obj));
        if (!it.second) {
            it.first.value() = std::forward<M>(obj);
        }

        return it;
    }

    template <typename P, class... Args>
    std::pair<iterator, bool> try_emplace_impl(P &&key, Args &&... args_value) {
        const std::size_t hash = hash_key(key);
        const std::size_t ibucket_for_hash = bucket_for_hash(hash);

        // Check if already presents
        auto it_find = find_impl(key, hash, m_first_or_empty_bucket + ibucket_for_hash);
        if (it_find != end()) {
            return std::make_pair(it_find, false);
        }

        return insert_impl(ibucket_for_hash, hash, std::piecewise_construct,
                           std::forward_as_tuple(std::forward<P>(key)),
                           std::forward_as_tuple(std::forward<Args>(args_value)...));
    }

    template <typename P>
    std::pair<iterator, bool> insert_impl(P &&value) {
        const std::size_t hash = hash_key(KeySelect()(value));
        const std::size_t ibucket_for_hash = bucket_for_hash(hash);

        // Check if already presents
        auto it_find =
            find_impl(KeySelect()(value), hash, m_first_or_empty_bucket + ibucket_for_hash);
        if (it_find != end()) {
            return std::make_pair(it_find, false);
        }

        return insert_impl(ibucket_for_hash, hash, std::forward<P>(value));
    }

    template <typename... Args>
    std::pair<iterator, bool>
    insert_impl(std::size_t ibucket_for_hash, std::size_t hash, Args &&... value_type_args) {
        if ((m_nb_elements - m_overflow_elements.size()) >= m_max_load_threshold_rehash) {
            rehash(GrowthPolicy::next_bucket_count());
            ibucket_for_hash = bucket_for_hash(hash);
        }

        std::size_t ibucket_empty = find_empty_bucket(ibucket_for_hash);
        if (ibucket_empty < m_buckets.size()) {
            do {
                tsl_assert(ibucket_empty >= ibucket_for_hash);

                // Empty bucket is in range of NeighborhoodSize, use it
                if (ibucket_empty - ibucket_for_hash < NeighborhoodSize) {
                    auto it = insert_in_bucket(ibucket_empty, ibucket_for_hash, hash,
                                               std::forward<Args>(value_type_args)...);
                    return std::make_pair(
                        iterator(it, m_buckets.end(), m_overflow_elements.begin()), true);
                }
            }
            // else, try to swap values to get a closer empty bucket
            while (swap_empty_bucket_closer(ibucket_empty));
        }

        // Load factor is too low or a rehash will not change the neighborhood, put the value in overflow list
        if (size() < m_min_load_threshold_rehash ||
            !will_neighborhood_change_on_rehash(ibucket_for_hash)) {
            auto it = insert_in_overflow(ibucket_for_hash, std::forward<Args>(value_type_args)...);
            return std::make_pair(iterator(m_buckets.end(), m_buckets.end(), it), true);
        }

        rehash(GrowthPolicy::next_bucket_count());
        ibucket_for_hash = bucket_for_hash(hash);

        return insert_impl(ibucket_for_hash, hash, std::forward<Args>(value_type_args)...);
    }

    /*
     * Return true if a rehash will change the position of a key-value in the neighborhood of 
     * ibucket_neighborhood_check. In this case a rehash is needed instead of puting the value in overflow list.
     */
    bool will_neighborhood_change_on_rehash(size_t ibucket_neighborhood_check) const {
        std::size_t expand_bucket_count = GrowthPolicy::next_bucket_count();
        GrowthPolicy expand_growth_policy(expand_bucket_count);

        const bool use_stored_hash = USE_STORED_HASH_ON_REHASH(expand_bucket_count);
        for (size_t ibucket = ibucket_neighborhood_check;
             ibucket < m_buckets.size() &&
             (ibucket - ibucket_neighborhood_check) < NeighborhoodSize;
             ++ibucket) {
            tsl_assert(!m_buckets[ibucket].empty());

            const size_t hash = use_stored_hash ? m_buckets[ibucket].truncated_bucket_hash()
                                                : hash_key(KeySelect()(m_buckets[ibucket].value()));
            if (bucket_for_hash(hash) != expand_growth_policy.bucket_for_hash(hash)) {
                return true;
            }
        }

        return false;
    }

    /*
     * Return the index of an empty bucket in m_buckets.
     * If none, the returned index equals m_buckets.size()
     */
    std::size_t find_empty_bucket(std::size_t ibucket_start) const {
        const std::size_t limit =
            std::min(ibucket_start + MAX_PROBES_FOR_EMPTY_BUCKET, m_buckets.size());
        for (; ibucket_start < limit; ibucket_start++) {
            if (m_buckets[ibucket_start].empty()) {
                return ibucket_start;
            }
        }

        return m_buckets.size();
    }

    /*
     * Insert value in ibucket_empty where value originally belongs to ibucket_for_hash
     * 
     * Return bucket iterator to ibucket_empty
     */
    template <typename... Args>
    iterator_buckets insert_in_bucket(std::size_t ibucket_empty,
                                      std::size_t ibucket_for_hash,
                                      std::size_t hash,
                                      Args &&... value_type_args) {
        tsl_assert(ibucket_empty >= ibucket_for_hash);
        tsl_assert(m_buckets[ibucket_empty].empty());
        m_buckets[ibucket_empty].set_value_of_empty_bucket(hopscotch_bucket::truncate_hash(hash),
                                                           std::forward<Args>(value_type_args)...);

        tsl_assert(!m_buckets[ibucket_for_hash].empty());
        m_buckets[ibucket_for_hash].toggle_neighbor_presence(ibucket_empty - ibucket_for_hash);
        m_nb_elements++;

        return m_buckets.begin() + ibucket_empty;
    }

    template <class... Args,
              class U = OverflowContainer,
              typename std::enable_if<!has_key_compare<U>::value>::type * = nullptr>
    iterator_overflow insert_in_overflow(std::size_t ibucket_for_hash, Args &&... value_type_args) {
        auto it = m_overflow_elements.emplace(m_overflow_elements.end(),
                                              std::forward<Args>(value_type_args)...);

        m_buckets[ibucket_for_hash].set_overflow(true);
        m_nb_elements++;

        return it;
    }

    template <class... Args,
              class U = OverflowContainer,
              typename std::enable_if<has_key_compare<U>::value>::type * = nullptr>
    iterator_overflow insert_in_overflow(std::size_t ibucket_for_hash, Args &&... value_type_args) {
        auto it = m_overflow_elements.emplace(std::forward<Args>(value_type_args)...).first;

        m_buckets[ibucket_for_hash].set_overflow(true);
        m_nb_elements++;

        return it;
    }

    /*
     * Try to swap the bucket ibucket_empty_in_out with a bucket preceding it while keeping the neighborhood 
     * conditions correct.
     * 
     * If a swap was possible, the position of ibucket_empty_in_out will be closer to 0 and true will re returned.
     */
    bool swap_empty_bucket_closer(std::size_t &ibucket_empty_in_out) {
        tsl_assert(ibucket_empty_in_out >= NeighborhoodSize);
        const std::size_t neighborhood_start = ibucket_empty_in_out - NeighborhoodSize + 1;

        for (std::size_t to_check = neighborhood_start; to_check < ibucket_empty_in_out;
             to_check++) {
            neighborhood_bitmap neighborhood_infos = m_buckets[to_check].neighborhood_infos();
            std::size_t to_swap = to_check;

            while (neighborhood_infos != 0 && to_swap < ibucket_empty_in_out) {
                if ((neighborhood_infos & 1) == 1) {
                    tsl_assert(m_buckets[ibucket_empty_in_out].empty());
                    tsl_assert(!m_buckets[to_swap].empty());

                    m_buckets[to_swap].swap_value_into_empty_bucket(
                        m_buckets[ibucket_empty_in_out]);

                    tsl_assert(!m_buckets[to_check].check_neighbor_presence(ibucket_empty_in_out -
                                                                            to_check));
                    tsl_assert(m_buckets[to_check].check_neighbor_presence(to_swap - to_check));

                    m_buckets[to_check].toggle_neighbor_presence(ibucket_empty_in_out - to_check);
                    m_buckets[to_check].toggle_neighbor_presence(to_swap - to_check);

                    ibucket_empty_in_out = to_swap;

                    return true;
                }

                to_swap++;
                neighborhood_infos = neighborhood_bitmap(neighborhood_infos >> 1);
            }
        }

        return false;
    }

    template <class K,
              class U = ValueSelect,
              typename std::enable_if<has_mapped_type<U>::value>::type * = nullptr>
    typename U::value_type *
    find_value_impl(const K &key, std::size_t hash, hopscotch_bucket *bucket_for_hash) {
        return const_cast<typename U::value_type *>(
            static_cast<const hopscotch_hash *>(this)->find_value_impl(key, hash, bucket_for_hash));
    }

    /*
     * Avoid the creation of an iterator to just get the value for operator[] and at() in maps. Faster this way.
     *
     * Return null if no value for the key (TODO use std::optional when available).
     */
    template <class K,
              class U = ValueSelect,
              typename std::enable_if<has_mapped_type<U>::value>::type * = nullptr>
    const typename U::value_type *
    find_value_impl(const K &key, std::size_t hash, const hopscotch_bucket *bucket_for_hash) const {
        const hopscotch_bucket *bucket_found = find_in_buckets(key, hash, bucket_for_hash);
        if (bucket_found != nullptr) {
            return std::addressof(ValueSelect()(bucket_found->value()));
        }

        if (bucket_for_hash->has_overflow()) {
            auto it_overflow = find_in_overflow(key);
            if (it_overflow != m_overflow_elements.end()) {
                return std::addressof(ValueSelect()(*it_overflow));
            }
        }

        return nullptr;
    }

    template <class K>
    size_type
    count_impl(const K &key, std::size_t hash, const hopscotch_bucket *bucket_for_hash) const {
        if (find_in_buckets(key, hash, bucket_for_hash) != nullptr) {
            return 1;
        } else if (bucket_for_hash->has_overflow() &&
                   find_in_overflow(key) != m_overflow_elements.cend()) {
            return 1;
        } else {
            return 0;
        }
    }

    template <class K>
    iterator find_impl(const K &key, std::size_t hash, hopscotch_bucket *bucket_for_hash) {
        hopscotch_bucket *bucket_found = find_in_buckets(key, hash, bucket_for_hash);
        if (bucket_found != nullptr) {
            return iterator(m_buckets.begin() + std::distance(m_buckets.data(), bucket_found),
                            m_buckets.end(), m_overflow_elements.begin());
        }

        if (!bucket_for_hash->has_overflow()) {
            return end();
        }

        return iterator(m_buckets.end(), m_buckets.end(), find_in_overflow(key));
    }

    template <class K>
    const_iterator
    find_impl(const K &key, std::size_t hash, const hopscotch_bucket *bucket_for_hash) const {
        const hopscotch_bucket *bucket_found = find_in_buckets(key, hash, bucket_for_hash);
        if (bucket_found != nullptr) {
            return const_iterator(m_buckets.cbegin() +
                                      std::distance(m_buckets.data(), bucket_found),
                                  m_buckets.cend(), m_overflow_elements.cbegin());
        }

        if (!bucket_for_hash->has_overflow()) {
            return cend();
        }

        return const_iterator(m_buckets.cend(), m_buckets.cend(), find_in_overflow(key));
    }

    template <class K>
    hopscotch_bucket *
    find_in_buckets(const K &key, std::size_t hash, hopscotch_bucket *bucket_for_hash) {
        const hopscotch_bucket *bucket_found =
            static_cast<const hopscotch_hash *>(this)->find_in_buckets(key, hash, bucket_for_hash);
        return const_cast<hopscotch_bucket *>(bucket_found);
    }

    /**
     * Return a pointer to the bucket which has the value, nullptr otherwise.
     */
    template <class K>
    const hopscotch_bucket *
    find_in_buckets(const K &key, std::size_t hash, const hopscotch_bucket *bucket_for_hash) const {
        (void) hash; // Avoid warning of unused variable when StoreHash is false;

        // TODO Try to optimize the function.
        // I tried to use ffs and  __builtin_ffs functions but I could not reduce the time the function
        // takes with -march=native

        neighborhood_bitmap neighborhood_infos = bucket_for_hash->neighborhood_infos();
        /*while (true) {
			switch (neighborhood_infos) {
				case 0:
					return nullptr;
					break;
				case 1:
					if((!StoreHash || bucket_for_hash->bucket_hash_equal(hash)) &&
					   compare_keys(KeySelect()(bucket_for_hash->value()), key))
					{
						return bucket_for_hash;
					} else {
						return nullptr;
					}
					break;
				default:
					if(neighborhood_infos & 1) {
						if((!StoreHash || bucket_for_hash->bucket_hash_equal(hash)) &&
						   compare_keys(KeySelect()(bucket_for_hash->value()), key))
						{
							return bucket_for_hash;
						}
						neighborhood_infos &= ~1;
					}
					static_assert(sizeof(uint32_t) >= sizeof(neighborhood_infos), "size not match");
					uint32_t index = __builtin_ctz(neighborhood_infos);
					neighborhood_infos = neighborhood_bitmap(neighborhood_infos >> index);
					bucket_for_hash += index;
					break;
			}
		}
		return nullptr;*/

        while (neighborhood_infos != 0) {
            if (neighborhood_infos & 1) {
                // Check StoreHash before calling bucket_hash_equal. Functionally it doesn't change anythin.
                // If StoreHash is false, bucket_hash_equal is a no-op. Avoiding the call is there to help
                // GCC optimizes `hash` parameter away, it seems to not be able to do without this hint.
                if ((!StoreHash || bucket_for_hash->bucket_hash_equal(hash)) &&
                    compare_keys(bucket_for_hash->value().first, key)) {
                    return bucket_for_hash;
                }
                if (neighborhood_infos == 1) {
                    return nullptr;
                }
                neighborhood_infos &= ~1;
            }
            uint32_t index = 0;
			// TODO: macOS
            if constexpr (sizeof(uint64_t) == sizeof(neighborhood_infos)) {
                index = __builtin_ctzll(neighborhood_infos);
            } else {
                index = __builtin_ctz(neighborhood_infos);
            }
            neighborhood_infos = neighborhood_bitmap(neighborhood_infos >> index);
            bucket_for_hash += index;
        }

        return nullptr;
    }

    template <class K,
              class U = OverflowContainer,
              typename std::enable_if<!has_key_compare<U>::value>::type * = nullptr>
    iterator_overflow find_in_overflow(const K &key) {
        return std::find_if(
            m_overflow_elements.begin(), m_overflow_elements.end(),
            [&](const value_type &value) { return compare_keys(key, KeySelect()(value)); });
    }

    template <class K,
              class U = OverflowContainer,
              typename std::enable_if<!has_key_compare<U>::value>::type * = nullptr>
    const_iterator_overflow find_in_overflow(const K &key) const {
        return std::find_if(
            m_overflow_elements.cbegin(), m_overflow_elements.cend(),
            [&](const value_type &value) { return compare_keys(key, KeySelect()(value)); });
    }

    template <class K,
              class U = OverflowContainer,
              typename std::enable_if<has_key_compare<U>::value>::type * = nullptr>
    iterator_overflow find_in_overflow(const K &key) {
        return m_overflow_elements.find(key);
    }

    template <class K,
              class U = OverflowContainer,
              typename std::enable_if<has_key_compare<U>::value>::type * = nullptr>
    const_iterator_overflow find_in_overflow(const K &key) const {
        return m_overflow_elements.find(key);
    }

    template <class U = OverflowContainer,
              typename std::enable_if<!has_key_compare<U>::value>::type * = nullptr>
    hopscotch_hash new_hopscotch_hash(size_type bucket_count) {
        return hopscotch_hash(bucket_count, static_cast<Hash &>(*this),
                              static_cast<KeyEqual &>(*this), get_allocator(), m_max_load_factor);
    }

    template <class U = OverflowContainer,
              typename std::enable_if<has_key_compare<U>::value>::type * = nullptr>
    hopscotch_hash new_hopscotch_hash(size_type bucket_count) {
        return hopscotch_hash(bucket_count, static_cast<Hash &>(*this),
                              static_cast<KeyEqual &>(*this), get_allocator(), m_max_load_factor,
                              m_overflow_elements.key_comp());
    }

public:
    static const size_type DEFAULT_INIT_BUCKETS_SIZE = 16;
    static constexpr float DEFAULT_MAX_LOAD_FACTOR = (NeighborhoodSize <= 30) ? 0.8f : 0.9f;

private:
    static const std::size_t MAX_PROBES_FOR_EMPTY_BUCKET = 12 * NeighborhoodSize;
    static constexpr float MIN_LOAD_FACTOR_FOR_REHASH = 0.1f;

    static bool USE_STORED_HASH_ON_REHASH(size_type bucket_count) {
        (void) bucket_count;
        if (StoreHash && is_power_of_two_policy<GrowthPolicy>::value) {
            tsl_assert(bucket_count > 0);
            return (bucket_count - 1) <= std::numeric_limits<truncated_hash_type>::max();
        } else {
            return false;
        }
    }

    /**
     * Return an always valid pointer to an static empty hopscotch_bucket.
     */
    hopscotch_bucket *static_empty_bucket_ptr() {
        static hopscotch_bucket empty_bucket;
        return &empty_bucket;
    }

private:
    buckets_container_type m_buckets;
    overflow_container_type m_overflow_elements;

    /**
     * Points to m_buckets.data() if !m_buckets.empty() otherwise points to static_empty_bucket_ptr.
     * This variable is useful to avoid the cost of checking if m_buckets is empty when trying 
     * to find an element.
     */
    hopscotch_bucket *m_first_or_empty_bucket;

    size_type m_nb_elements;

    float m_max_load_factor;

    /**
     * Max size of the hash table before a rehash occurs automatically to grow the table.
     */
    size_type m_max_load_threshold_rehash;

    /**
     * Min size of the hash table before a rehash can occurs automatically (except if m_max_load_threshold_rehash os reached).
     * If the neighborhood of a bucket is full before the min is reacher, the elements are put into m_overflow_elements.
     */
    size_type m_min_load_threshold_rehash;
};

} // end namespace detail_hopscotch_hash

} // end namespace tsl

#endif
