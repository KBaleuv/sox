/* 
 * File:   serialization.hpp
 * Author: Konstantin
 *
 * Created on June 9, 2016, 5:14 PM
 */

#ifndef SERIALIZATION_HPP
#define	SERIALIZATION_HPP

#include <type_traits>

namespace data
{
    template <typename T, typename U = char>
    struct basic_sequence
    {
        static_assert(std::is_trivial<T>::value, "Main type must be trivial");
        static_assert(sizeof(T) / sizeof(U) * sizeof(U) == sizeof(T),
                "Basic type size must be a divisor of the main type");

        typedef T type_t;
        typedef U basic_t;
        
        basic_sequence() {}
        basic_sequence(const type_t& x) : value(x) {}
        basic_sequence(type_t&& x) : value(x) {}

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
        
        SerializableSequence() {}
        SerializableSequence(const type_t& x) : basic_sequence<T, U>(x) {}
        SerializableSequence(type_t&& x) : basic_sequence<T, U>(x) {}

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
};

#endif	/* SERIALIZATION_HPP */

