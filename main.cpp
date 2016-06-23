/* 
 * File:   main.cpp
 * Author: Konstantin
 *
 * Created on June 4, 2016, 8:09 AM
 */

#include <iostream>
#include <type_traits>
#include <tuple>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <typeinfo>
#include <set>
#include <sstream>

#include "data/basic_binder.hpp"

template <typename T>
struct is_forward_sequence
{
    template <typename A>
    static std::true_type test (decltype(std::begin(std::declval<A>()))*, decltype(std::end(std::declval<A>()))*);
    
    template <typename A>
    static std::false_type test (...);
    
    typedef decltype(test<T>(nullptr, nullptr)) type;
    static const bool value = type::value;
};

template <typename T>
struct is_tuple
{
    typedef std::false_type type;
    static const bool value = type::value;
};

template <typename... _Ty>
struct is_tuple<std::tuple<_Ty...>>
{
    typedef std::true_type type;
    static const bool value = type::value;
};

template <typename... _Ty>
struct is_tuple<std::pair<_Ty...>>
{
    typedef std::true_type type;
    static const bool value = type::value;
};

template <typename T, typename U = char>
struct basic_sequence
{
    static_assert(std::is_trivial<T>::value, "Main type must be trivial");
    static_assert(sizeof(T) / sizeof(U) * sizeof(U) == sizeof(T),
            "Basic type size must be a divisor of the main type");
    
    typedef T type_t;
    typedef U basic_t;
    
    static const size_t length = sizeof(T) / sizeof(U);
    
    union
    {
        T value;
        U sequence [length];
    };
};

struct Endian
{
    typedef uint64_t probe_t;
    
    static size_t endian_length()
    {
        volatile basic_sequence<probe_t, uint8_t> probe;
        probe.value = 1;
        size_t length = 0;
        while (length < probe.length)
        {
            if (probe.sequence[length] == 1) break;
        }
        return length + 1;
    }
    
    static size_t length;
    static bool is_big;
};

size_t Endian::length = endian_length();
bool Endian::is_big = (length != 1);

template <typename T, typename U>
struct SerializableSequence : public basic_sequence<T, U>
{
    typedef T type_t;
    typedef U basic_t;
    
    SerializableSequence& serialize()
    {
        if (!Endian::is_big)
        {
            for (basic_t* begin = basic_sequence<T, U>::sequence, *end = basic_sequence<T, U>::sequence + basic_sequence<T, U>::length - 1; begin < end; ++begin, --end)
            {
                std::swap(*begin, *end);
            }
        }
        return *this;
    }
};

struct SingleSaver
{
    template <typename T, typename _Tch, typename _Ttr>
    typename std::enable_if<std::is_trivial<T>::value, std::basic_ostream<_Tch, _Ttr>&>::type operator() (std::basic_ostream<_Tch, _Ttr>& stream, const T& x)
    {
        SerializableSequence<T, _Tch> serializer;
        serializer.value = x;
        serializer.serialize();
        return stream.write(serializer.sequence, serializer.length); 
    }
};

template <typename _Lt, typename _H>
struct TypeInfo
{
    typedef _Lt length_t;
    typedef _H hash_t;
    
    TypeInfo() : type(), size(), m_embedded_types() {}
    
    enum class StoredType : uint8_t {Regular, Sequence, Tuple};
    
    StoredType type;
    length_t size;
    std::vector<hash_t> m_embedded_types;
};

template <typename _Ss, typename _Mp>
struct SequenceSaver
{
    typedef _Ss single_saver_t;
    typedef _Mp meta_provider_t;

    template <typename T, typename _Tch, typename _Ttr>
    std::basic_ostream<_Tch, _Ttr>& operator()(std::basic_ostream<_Tch, _Ttr>& stream, const T& x)
    {
        save_silent(stream, m_meta_provider.id(x));
        save(stream, x);
    }
    
protected:
    template <typename T, typename _Tch, typename _Ttr>
    typename std::enable_if<is_forward_sequence<T>::value, std::basic_ostream<_Tch, _Ttr>&>::type save_silent (std::basic_ostream<_Tch, _Ttr>& stream, const T& seq)
    {
        for (auto iter = std::begin(seq); iter != std::end(seq); ++iter)
        {
            save_silent(stream, *iter);
        }
        return stream;
    }
    
    template <typename T, typename _Tch, typename _Ttr>
    typename std::enable_if<is_forward_sequence<T>::value, std::basic_ostream<_Tch, _Ttr>&>::type save (std::basic_ostream<_Tch, _Ttr>& stream, const T& seq)
    {
        save_silent(stream, m_meta_provider.length(seq));
        save_silent(stream, seq);
    }
    
    template <typename T, typename _Tch, typename _Ttr>
    typename std::enable_if<is_tuple<T>::value, std::basic_ostream<_Tch, _Ttr>&>::type save (std::basic_ostream<_Tch, _Ttr>& stream, const T& x)
    {
        save_silent(stream, x);
    }
    
    template <typename T, typename _Tch, typename _Ttr, size_t I = 0>
    typename std::enable_if<is_tuple<T>::value && (I < std::tuple_size<T>::value), std::basic_ostream<_Tch, _Ttr>&>::type save_silent (std::basic_ostream<_Tch, _Ttr>& stream, const T& tpl)
    {
        save(stream, std::get<I>(tpl));
        return save_silent<T, _Tch, _Ttr, I + 1>(stream, tpl);
    }
    
    template <typename T, typename _Tch, typename _Ttr, size_t I = 0>
    typename std::enable_if<is_tuple<T>::value && (I >= std::tuple_size<T>::value), std::basic_ostream<_Tch, _Ttr>&>::type save_silent (std::basic_ostream<_Tch, _Ttr>& stream, const T& tpl)
    {
        return stream;
    }
    
    template <typename T, typename _Tch, typename _Ttr>
    typename std::enable_if<!(is_forward_sequence<T>::value || is_tuple<T>::value), std::basic_ostream<_Tch, _Ttr>&>::type save (std::basic_ostream<_Tch, _Ttr>& stream, const T& x)
    {
        save_silent(stream, x);
    }
    
    template <typename T, typename _Tch, typename _Ttr>
    typename std::enable_if<!(is_forward_sequence<T>::value || is_tuple<T>::value), std::basic_ostream<_Tch, _Ttr>&>::type save_silent (std::basic_ostream<_Tch, _Ttr>& stream, const T& x)
    {
        m_single_saver(stream, x);
        return stream;
    }
protected:
    single_saver_t m_single_saver;
    meta_provider_t m_meta_provider;
};

template <typename _Ss, typename _Mp, typename _Lt, typename _Idt>
struct TypeInfoSaver : protected SequenceSaver<_Ss, _Mp>
{
    typedef _Ss single_saver_t;
    typedef _Mp metadata_provider_t;
    typedef SequenceSaver<_Ss, _Mp> sequence_saver_t;
    typedef TypeInfo<_Lt, _Idt> type_info_t;
    
    template <typename _Tch, typename _Ttr>
    std::basic_ostream<_Tch, _Ttr>& operator() (std::basic_ostream<_Tch, _Ttr>& stream, const type_info_t& info)
    {
        sequence_saver_t::save_silent(stream, static_cast<typename std::underlying_type<decltype(info.type)>::type>(info.type));
        if (info.type == type_info_t::StoredType::Regular || info.type == type_info_t::StoredType::Tuple)
        {
            sequence_saver_t::save_silent(stream, info.size);
        }
        sequence_saver_t::save_silent(stream, info.m_embedded_types);
    }
    
    template <typename _Tch, typename _Ttr>
    std::basic_istream<_Tch, _Ttr>& operator() (std::basic_istream<_Tch, _Ttr>& stream, type_info_t& info)
    {
        
    }
};

template <typename K, typename H = std::hash<K>>
struct HashKey
{
    typedef K key_t;
    typedef H hasher_t;
    typedef decltype(std::declval<H>()(std::declval<K>())) hash_t;
    
    HashKey() : m_key(), m_hasher(), m_hash() {}
    HashKey(const key_t& key) : m_key(key), m_hasher(), m_hash(m_hasher(m_key)) {}
    HashKey(const key_t& key, const hasher_t& hasher) : m_key(key), m_hasher(hasher), m_hash(m_hasher(m_key)) {}
    HashKey(const hash_t& hash) : m_key(), m_hasher(), m_hash(hash) {}
    HashKey(const hash_t& hash, const hasher_t& hasher) : m_key(), m_hasher(hasher), m_hash(hash) {}
    ~HashKey() {}
    
    void key(const key_t& x)
    {
        m_key = x;
        if (m_hasher)
        {
            m_hash = m_hasher(m_key);
        }
    }
    
    const key_t& key() const
    {
        return m_key;
    }
    
    void hasher(const hasher_t& x)
    {
        m_hasher = x;
        if (m_key)
        {
            m_hash = m_hasher(m_key);
        }
    }
    
    const hasher_t& hasher() const
    {
        return m_hasher;
    }
    
    void hash(const hash_t& x)
    {
        m_hash = x;
        if (m_key)
        {
            m_key = key_t();
        }
    }
    
    const hash_t& hash() const
    {
        return m_hash;
    }
    
    operator hash_t () const
    {
        return m_hash;
    }
    
    operator key_t () const
    {
        return m_key;
    }
    
protected:
    key_t m_key;
    hasher_t m_hasher;
    hash_t m_hash;
};

struct TypeKey : public HashKey<std::string>
{
    template <typename T>
    TypeKey(const T& x) : HashKey(typeid(T).name()) {}
    ~TypeKey() {}
};

template <typename T>
bool operator< (const HashKey<T>& lhs, const HashKey<T>& rhs)
{
    return (lhs.hash() < rhs.hash());
}

template <typename _H, typename _Lt, typename _Idt = _Lt>
struct TypeInfoProvider
{
    typedef _H hash_t;
    typedef _Lt length_t;
    typedef _Idt id_t;
    
    template <typename T>
    id_t id (const T& x)
    {
        hash_t key (operator()(x));
        return id_t (key);
    }
    
    template <typename T>
    typename std::enable_if<is_forward_sequence<T>::value, length_t>::type length (const T& x)
    {
        length_t length (0);
        for (auto iter = std::begin(x); iter != std::end(x); ++iter, ++length);
        return length;
    }
    
    TypeInfo<length_t, hash_t> operator() (const hash_t& hash)
    {
        auto iter = m_info.find(hash);
        if (iter == std::end(m_info))
        {
            throw std::out_of_range("Info on the requested type is unavailable.");
        }
        else
        {
            return iter->second;
        }
    }
    
    template <typename T>
    hash_t operator() (const T& x)
    {
        auto iter = m_info.find(x);
        if (iter == std::end(m_info))
        {
            return emplace_type(x);
        }
        else
        {
            return *iter;
        }
    }
    
    template <typename T>
    typename std::enable_if<is_forward_sequence<T>::value, hash_t>::type emplace_type (const T& x)
    {
        TypeInfo<length_t, hash_t> info;
        info.type = TypeInfo<length_t, hash_t>::StoredType::Sequence;
        info.size = 0;
        info.m_embedded_types.emplace_back(operator()(*std::begin(x)));
        hash_t key (x);
        m_info.emplace(key, info);
        return key;
    }
    
    template <typename T>
    typename std::enable_if<!is_forward_sequence<T>::value, hash_t>::type emplace_type (const T& x)
    {
        TypeInfo<length_t, hash_t> info;
        info.type = TypeInfo<length_t, hash_t>::StoredType::Regular;
        info.size = sizeof(T);
        hash_t key (x);
        m_info.emplace(key, info);
        return key;
    }
    
    template <typename... A>
    hash_t emplace_type (const std::tuple<A...>& x)
    {
        TypeInfo<length_t, hash_t> info;
        info.type = TypeInfo<length_t, hash_t>::StoredType::Tuple;
        info.size = sizeof...(A);
        emplace_tuple_type(x, info);
        hash_t key (x);
        m_info.emplace(key, info);
        return key;
    }
    
    template <typename _T1, typename _T2>
    hash_t emplace_type (const std::pair<_T1, _T2>& x)
    {
        TypeInfo<length_t, hash_t> info;
        info.type = TypeInfo<length_t, hash_t>::StoredType::Tuple;
        info.size = 2;
        info.m_embedded_types.emplace_back(operator()(x.first));
        info.m_embedded_types.emplace_back(operator()(x.second));
        hash_t key (x);
        m_info.emplace(key, info);
        return key;
    }
    
protected:
    std::map<hash_t, TypeInfo<length_t, hash_t>> m_info;
private:
    template <size_t I = 0, typename... A>
    typename std::enable_if<(I < sizeof...(A)), void>::type emplace_tuple_type (const std::tuple<A...>& x, TypeInfo<length_t, hash_t>& info)
    {
        info.m_embedded_types.emplace_back(operator()(std::get<I>(x)));
        emplace_tuple_type<I + 1, A...>(x, info);
    }
    
    template <size_t I = 0, typename... A>
    typename std::enable_if<(I == sizeof...(A)), void>::type emplace_tuple_type (const std::tuple<A...>& x, TypeInfo<length_t, hash_t>& info) {}
};

typedef SingleSaver default_single_saver;
typedef TypeKey default_hash_type;
typedef size_t default_length_type;
typedef size_t default_id_type;
typedef TypeInfoProvider<default_hash_type, default_length_type, default_id_type> native_provider;
typedef SequenceSaver<default_single_saver, native_provider> native_saver;

struct dummy_type {};

int main (int argc, char** argv)
{
    data::composite_binder<data::mock, data::tuple_binder, data::sequence_binder<size_t>, data::trivial_binder> saver;
    //native_saver saver;
    //data::mock mockup;
    std::stringstream buffer;
    std::fstream dbFile ("./dbfile.img", std::ios::binary | std::ios::out);
    std::map<std::string, std::tuple<std::string, int>> map;
    map["Sample"] = std::tuple<std::string, int> {"Containing string...", 32};
    map["another"] = std::tuple<std::string, int> {"string is ambiguous", 255};
    saver(static_cast<std::ostream&>(dbFile), map);
    dbFile.close();
    std::cout << "Initial: ";
    saver(std::cout, map);
    saver(buffer, map);
    std::cout << std::endl << "Cleared: ";
    map.clear();
    saver(std::cout, map);
    std::cout << std::endl << "Refilled: ";
    saver(map, buffer);
    saver(std::cout, map);
    std::cout << std::endl;
    std::tuple<std::string, int> found = map["another"];
    std::cout << "map[\"another\"] -> [" << std::get<0>(found) << ", " << std::get<1>(found) << "]" << std::endl;
    return 0;
}

