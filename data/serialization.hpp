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
            volatile probe_t probe (1);
            volatile uint8_t* volatile ptr_base = reinterpret_cast<volatile uint8_t*>(&probe);
            volatile uint8_t* volatile ptr = ptr_base;
            while (*ptr != 1)
            {
                ++ptr;
            }
            return (ptr - ptr_base) + 1;
        }

        static size_t length;
        static bool is_big;
    };

    size_t Endian::length = endian_length();
    bool Endian::is_big = (length == 1);
    
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
            return to_big();
        }
        
        SerializableSequence& to_big()
        {
            if (!Endian::is_big)
            {
                __swap_order();
            }
            return *this;
        }
        
        SerializableSequence& to_little()
        {
            if (Endian::is_big)
            {
                __swap_order();
            }
            return *this;
        }
    private:
        void __swap_order()
        {
            for (basic_t* begin = basic_sequence<T, U>::sequence, *end = basic_sequence<T, U>::sequence + basic_sequence<T, U>::length - 1; begin < end; ++begin, --end)
            {
                std::swap(*begin, *end);
            }
        }
    };
    
    struct length_type
    {
        template <typename T>
        operator size_t()
        {
            return value;
        }
        
        size_t value;
    };
    
    length_type& operator++ (length_type& lhs)
    {
        ++lhs.value;
        return lhs;
    }
    
    length_type operator++ (length_type& lhs, int)
    {
        length_type temp;
        temp.value = lhs.value;
        ++lhs.value;
        return temp;
    }
    
    length_type& operator-- (length_type& lhs)
    {
        --lhs.value;
        return lhs;
    }
    
    length_type operator-- (length_type& lhs, int)
    {
        length_type temp;
        temp.value = lhs.value;
        --lhs.value;
        return temp;
    }
    
    template <typename T>
    bool operator> (length_type lhs, T rhs)
    {
        return lhs.value > rhs;
    }
    
    template <typename T>
    bool operator> (T lhs, length_type rhs)
    {
        return lhs > rhs.value;
    }
    
    template <typename T>
    bool operator> (length_type lhs, length_type rhs)
    {
        return lhs.value > rhs.value;
    }
};

#endif	/* SERIALIZATION_HPP */

