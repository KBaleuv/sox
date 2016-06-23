/* 
 * File:   basic_binder.hpp
 * Author: Konstantin
 *
 * Created on June 8, 2016, 10:15 PM
 */

#ifndef BASIC_BINDER_HPP
#define	BASIC_BINDER_HPP

#include <iostream>
#include <tuple>
#include <type_traits>
#include <initializer_list>
#include "./serialization.hpp"

namespace data
{
    template <typename T, typename U, typename S>
    struct can_bind
    {
        typedef T binder_t;
        typedef typename std::add_lvalue_reference<typename std::decay<U>::type>::type base_t;
        typedef typename std::add_lvalue_reference<typename std::decay<S>::type>::type source_t;

        template <typename A>
        static const std::true_type __test (typename std::decay<decltype(std::declval<A>()(std::declval<source_t>(), std::declval<base_t>()))>::type*);

        template <typename A>
        static const std::false_type __test (...);

        typedef decltype(__test<binder_t>(nullptr)) type;
        static const bool value = type::value;
    };

    template <typename T, typename U, typename S, typename Cb>
    struct can_bind_with_callback
    {
        typedef T binder_t;
        typedef typename std::add_lvalue_reference<typename std::decay<U>::type>::type base_t;
        typedef typename std::add_lvalue_reference<typename std::decay<S>::type>::type source_t;
        typedef typename std::add_lvalue_reference<typename std::decay<Cb>::type>::type callback_t;
        
        template <typename A>
        static const std::true_type __test (typename std::decay<decltype(std::declval<A>()(std::declval<source_t>(), std::declval<base_t>(), std::declval<callback_t>()))>::type*);

        template <typename A>
        static const std::false_type __test (...);

        typedef decltype(__test<binder_t>(nullptr)) type;
        static const bool value = type::value;
    };

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

    template <typename... Types>
    struct construct_tuple
    {
        template <typename... Args>
        constexpr std::tuple<Types...> operator() (Args&&... args) const
        {
            return std::tuple<Types...> (Types(std::forward<Args>(args)...)...);
        }
    };

    template <typename _St = size_t>
    struct sequence_binder
    {
        typedef _St size_type;

        template <typename T, typename _Tch, typename _Ttr, typename Cb>
        typename std::enable_if<is_forward_sequence<T>::value, std::basic_ostream<_Tch, _Ttr>&>::type
        operator() (std::basic_ostream<_Tch, _Ttr>& stream, T&& x, Cb&& callback) const
        {
            size_type length {};
            for (auto iter = std::begin(std::forward<T>(x)); iter != std::end(std::forward<T>(x)); ++iter, ++length);
            callback(stream, length);
            for (auto iter = std::begin(std::forward<T>(x)); iter != std::end(std::forward<T>(x)); ++iter)
            {
                callback(stream, *iter);
            }
            return stream;
        }

        template <typename T, typename _Tch, typename _Ttr, typename Cb>
        typename std::enable_if<is_forward_sequence<T>::value, T&>::type
        operator() (T& x, std::basic_istream<_Tch, _Ttr>& stream, Cb&& callback) const
        {
            typedef typename std::decay<decltype(*std::begin(x))>::type underlying_t;
            typedef typename std::decay<T>::type type_t;
            
            size_type length {};
            callback(length, stream);
            std::vector<underlying_t> init_list;
            underlying_t buffer;
            while (length > 0)
            {
                callback(buffer, stream);
                init_list.push_back(buffer);
                --length;
            }
            type_t* ptr = reinterpret_cast<type_t*>((void*)&x);
            __free_object(ptr);
            new(ptr) type_t(std::begin(init_list), std::end(init_list));
            return x;
        }
    private:
        template <typename T>
        static typename std::enable_if<std::is_trivially_destructible<T>::value>::type __free_object (T* ptr)
        {}
        
        template <typename T>
        static typename std::enable_if<!std::is_trivially_destructible<T>::value>::type __free_object (T* ptr)
        {
            ptr->~T();
        }
    };

    struct tuple_binder
    {
        template <typename S, typename Cb, typename... A>
        S& operator() (S& stream, const std::tuple<A...>& x, Cb&& callback) const
        {
            return __bind_tuple(stream, x, std::forward<Cb>(callback));
        }
        template <typename S, typename Cb, typename T1, typename T2>
        S& operator() (S& stream, const std::pair<T1, T2>& x, Cb&& callback) const
        {
            callback(stream, x.first);
            callback(stream, x.second);
            return stream;
        }
        template <typename S, typename Cb, typename... A>
        std::tuple<A...>& operator() (std::tuple<A...>& x, S& stream, Cb&& callback) const
        {
            return __bind_tuple(x, stream, std::forward<Cb>(callback));
        }
        template <typename S, typename Cb, typename T1, typename T2>
        std::pair<T1, T2>& operator() (std::pair<T1, T2>& x, S& stream, Cb&& callback) const
        {
            callback(x.first, stream);
            callback(x.second, stream);
            return x;
        }
    private:
        template <size_t I = 0, typename S, typename Cb, typename... A>
        typename std::enable_if<I < sizeof...(A), S&>::type
        __bind_tuple (S& stream, const std::tuple<A...>& x, Cb&& callback) const
        {
            callback(stream, std::get<I>(x));
            return __bind_tuple<I + 1, S, Cb, A...> (stream, x, callback);
        }
        
        template <size_t I = 0, typename S, typename Cb, typename... A>
        typename std::enable_if<I == sizeof...(A), S&>::type
        __bind_tuple (S& stream, const std::tuple<A...>& x, Cb&& callback) const
        {
            return stream;
        }
        template <size_t I = 0, typename S, typename Cb, typename... A>
        typename std::enable_if<I < sizeof...(A), std::tuple<A...>&>::type
        __bind_tuple (std::tuple<A...>& x, S& stream, Cb&& callback) const
        {
            callback(std::get<I>(x), stream);
            return __bind_tuple<I + 1, S, Cb, A...> (x, stream, callback);
        }
        
        template <size_t I = 0, typename S, typename Cb, typename... A>
        typename std::enable_if<I == sizeof...(A), std::tuple<A...>&>::type
        __bind_tuple (std::tuple<A...>& x, S& stream, Cb&& callback) const
        {
            return x;
        }
    };
    
    struct trivial_binder
    {
        template <typename T, typename _Tch, typename _Ttr>
        typename std::enable_if<std::is_scalar<T>::value, std::basic_ostream<_Tch, _Ttr>&>::type
        operator() (std::basic_ostream<_Tch, _Ttr>& stream, const T& x) const
        {
            SerializableSequence<T, _Tch> serializer;
            serializer.value = x;
            serializer.serialize();
            return stream.write(serializer.sequence, serializer.length); 
        }
        
        template <typename T, typename _Tch, typename _Ttr>
        typename std::enable_if<std::is_scalar<T>::value, T&>::type
        operator() (T& x, std::basic_istream<_Tch, _Ttr>& stream) const
        {
            SerializableSequence<T, _Tch> serializer;
            stream.read(serializer.sequence, serializer.length);
            serializer.serialize();
            x = serializer.value;
            return x;
        }
    };

    template <typename _Provider, typename _Binder, typename... _Binders>
    struct composite_binder
    {
    private:
        template <typename _Ti, _Ti I, typename S, typename T, typename... A>
        struct __binder_index;

        template <typename _Ti, _Ti I, typename S, typename T, typename B, typename... A>
        struct __binder_index<_Ti, I, S, T, B, A...>
        {
            typedef typename std::add_lvalue_reference<typename std::decay<B>::type>::type binder_t;
            typedef typename std::add_lvalue_reference<typename std::decay<T>::type>::type type_t;
            
            static const _Ti value = can_bind<B, T, S>::value ? I : __binder_index<_Ti, I + 1, S, T, A...>::value;
            static const bool bindable = can_bind<B, T, S>::value ? true : __binder_index<_Ti, I + 1, S, T, A...>::bindable;
        };

        template <typename _Ti, _Ti I, typename S, typename T>
        struct __binder_index<_Ti, I, S, T>
        {
            static const _Ti value = 0;
            static const bool bindable = false;
        };
        
        template <typename _Ti, _Ti I, typename Cb, typename S, typename T, typename... A>
        struct __binder_index_w_callback;

        template <typename _Ti, _Ti I, typename Cb, typename S, typename T, typename B, typename... A>
        struct __binder_index_w_callback<_Ti, I, Cb, S, T, B, A...>
        {
            typedef typename std::add_lvalue_reference<typename std::decay<B>::type>::type binder_t;
            typedef typename std::add_lvalue_reference<typename std::decay<T>::type>::type type_t;
            typedef typename std::add_lvalue_reference<typename std::decay<Cb>::type>::type callback_t;
            
            static const _Ti value = can_bind_with_callback<B, T, S, Cb>::value ? I : __binder_index_w_callback<_Ti, I + 1, Cb, S, T, A...>::value;
            static const bool bindable = can_bind_with_callback<B, T, S, Cb>::value ? true : __binder_index_w_callback<_Ti, I + 1, Cb, S, T, A...>::bindable;
        };

        template <typename _Ti, _Ti I, typename Cb, typename S, typename T>
        struct __binder_index_w_callback<_Ti, I, Cb, S, T>
        {
            static const _Ti value = 0;
            static const bool bindable = false;
        };
    public:
        typedef std::tuple<_Binder, _Binders...> stored_t;
        typedef composite_binder<_Provider, _Binder, _Binders...> this_type;
        
        composite_binder() : m_provider(), m_binders() {}
        template <typename... Args>
        composite_binder(Args&&... args) {}
        template <typename Provider>
        composite_binder(Provider&& provider) : m_provider(std::forward<Provider>(provider)), m_binders() {}

        template <typename S, typename T>
        typename std::enable_if<__binder_index<size_t, 0, S, T, _Binder, _Binders...>::bindable, S&>::type
        operator() (S& binding, T&& x)
        {
            std::get<__binder_index<size_t, 0, S, T, _Binder, _Binders...>::value>(m_binders)(binding, std::forward<T>(x));
            return binding;
        }
        template <typename S, typename T>
        typename std::enable_if<!__binder_index<size_t, 0, S, T, _Binder, _Binders...>::bindable, S&>::type
        operator() (S& binding, T&& x)
        {
            std::get<__binder_index_w_callback<size_t, 0, this_type, S, T, _Binder, _Binders...>::value>(m_binders)(binding, std::forward<T>(x), *this);
            return binding;
        }
    protected:
        _Provider m_provider;
        stored_t m_binders;
    };
    
    struct mock
    {
        template <typename... Args>
        void operator()(Args&&...) {}
    };
};

#endif	/* BASIC_BINDER_HPP */

