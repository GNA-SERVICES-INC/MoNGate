// -*- C++ -*- header.

// Copyright (C) 2008, 2009, 2010
// Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Under Section 7 of GPL version 3, you are granted additional
// permissions described in the GCC Runtime Library Exception, version
// 3.1, as published by the Free Software Foundation.

// You should have received a copy of the GNU General Public License and
// a copy of the GCC Runtime Library Exception along with this program;
// see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
// <http://www.gnu.org/licenses/>.

/** @file bits/atomic_0.h
 *  This is an internal header file, included by other library headers.
 *  Do not attempt to use it directly. @headername{atomic}
 */

#ifndef _GLIBCXX_ATOMIC_0_H
#define _GLIBCXX_ATOMIC_0_H 1

#pragma GCC system_header

_GLIBCXX_BEGIN_NAMESPACE(std)

// 0 == __atomic0 == Never lock-free
namespace __atomic0
{
  _GLIBCXX_BEGIN_EXTERN_C

  void
  atomic_flag_clear_explicit(__atomic_flag_base*, memory_order)
  _GLIBCXX_NOTHROW;

  void
  __atomic_flag_wait_explicit(__atomic_flag_base*, memory_order)
  _GLIBCXX_NOTHROW;

  _GLIBCXX_CONST __atomic_flag_base*
  __atomic_flag_for_address(const volatile void* __z) _GLIBCXX_NOTHROW;

  _GLIBCXX_END_EXTERN_C

  // Implementation specific defines.
#define _ATOMIC_MEMBER_ _M_i

  // Implementation specific defines.
#define _ATOMIC_LOAD_(__a, __x)						   \
  ({typedef __typeof__(_ATOMIC_MEMBER_) __i_type;                          \
    __i_type* __p = &_ATOMIC_MEMBER_;	   				   \
    __atomic_flag_base* __g = __atomic_flag_for_address(__p);	  	   \
    __atomic_flag_wait_explicit(__g, __x);				   \
    __i_type __r = *__p;						   \
    atomic_flag_clear_explicit(__g, __x);		       		   \
    __r; })

#define _ATOMIC_STORE_(__a, __m, __x)					   \
  ({typedef __typeof__(_ATOMIC_MEMBER_) __i_type;                          \
    __i_type* __p = &_ATOMIC_MEMBER_;	   				   \
    __typeof__(__m) __v = (__m);			       		   \
    __atomic_flag_base* __g = __atomic_flag_for_address(__p);	  	   \
    __atomic_flag_wait_explicit(__g, __x);				   \
    *__p = __v;								   \
    atomic_flag_clear_explicit(__g, __x);		       		   \
    __v; })

#define _ATOMIC_MODIFY_(__a, __o, __m, __x)				   \
  ({typedef __typeof__(_ATOMIC_MEMBER_) __i_type;                          \
    __i_type* __p = &_ATOMIC_MEMBER_;	   				   \
    __typeof__(__m) __v = (__m);			       		   \
    __atomic_flag_base* __g = __atomic_flag_for_address(__p);	  	   \
    __atomic_flag_wait_explicit(__g, __x);				   \
    __i_type __r = *__p;		       				   \
    *__p __o __v;					       		   \
    atomic_flag_clear_explicit(__g, __x);		       		   \
    __r; })

#define _ATOMIC_CMPEXCHNG_(__a, __e, __m, __x)				   \
  ({typedef __typeof__(_ATOMIC_MEMBER_) __i_type;                          \
    __i_type* __p = &_ATOMIC_MEMBER_;	   				   \
    __typeof__(__e) __q = (__e);			       		   \
    __typeof__(__m) __v = (__m);			       		   \
    bool __r;						       		   \
    __atomic_flag_base* __g = __atomic_flag_for_address(__p);	   	   \
    __atomic_flag_wait_explicit(__g, __x);				   \
    __i_type __t = *__p;		       				   \
    if (*__q == __t) 							   \
    {                                                                      \
      *__p = (__i_type)__v;						\
      __r = true;                                                          \
    }									   \
    else { *__q = __t; __r = false; }		       			   \
    atomic_flag_clear_explicit(__g, __x);		       		   \
    __r; })


  /// atomic_flag
  struct atomic_flag : public __atomic_flag_base
  {
    atomic_flag() = default;
    ~atomic_flag() = default;
    atomic_flag(const atomic_flag&) = delete;
    atomic_flag& operator=(const atomic_flag&) = delete;
    atomic_flag& operator=(const atomic_flag&) volatile = delete;

    // Conversion to ATOMIC_FLAG_INIT.
    atomic_flag(bool __i): __atomic_flag_base({ __i }) { }

    bool
    test_and_set(memory_order __m = memory_order_seq_cst);

    bool
    test_and_set(memory_order __m = memory_order_seq_cst) volatile;

    void
    clear(memory_order __m = memory_order_seq_cst);

    void
    clear(memory_order __m = memory_order_seq_cst) volatile;
  };


  /// atomic_address
  struct atomic_address
  {
  private:
    void* _M_i;

  public:
    atomic_address() = default;
    ~atomic_address() = default;
    atomic_address(const atomic_address&) = delete;
    atomic_address& operator=(const atomic_address&) = delete;
    atomic_address& operator=(const atomic_address&) volatile = delete;

    constexpr atomic_address(void* __v): _M_i (__v) { }

    bool
    is_lock_free() const { return false; }

    bool
    is_lock_free() const volatile { return false; }

    void
    store(void* __v, memory_order __m = memory_order_seq_cst)
    {
      __glibcxx_assert(__m != memory_order_acquire);
      __glibcxx_assert(__m != memory_order_acq_rel);
      __glibcxx_assert(__m != memory_order_consume);
      _ATOMIC_STORE_(this, __v, __m);
    }

    void
    store(void* __v, memory_order __m = memory_order_seq_cst) volatile
    {
      __glibcxx_assert(__m != memory_order_acquire);
      __glibcxx_assert(__m != memory_order_acq_rel);
      __glibcxx_assert(__m != memory_order_consume);
      _ATOMIC_STORE_(this, __v, __m);
    }

    void*
    load(memory_order __m = memory_order_seq_cst) const
    {
      __glibcxx_assert(__m != memory_order_release);
      __glibcxx_assert(__m != memory_order_acq_rel);
      return _ATOMIC_LOAD_(this, __m);
    }

    void*
    load(memory_order __m = memory_order_seq_cst) const volatile
    {
      __glibcxx_assert(__m != memory_order_release);
      __glibcxx_assert(__m != memory_order_acq_rel);
      return _ATOMIC_LOAD_(this, __m);
    }

    void*
    exchange(void* __v, memory_order __m = memory_order_seq_cst)
    { return _ATOMIC_MODIFY_(this, =, __v, __m); }

    void*
    exchange(void* __v, memory_order __m = memory_order_seq_cst) volatile
    { return _ATOMIC_MODIFY_(this, =, __v, __m); }

    bool
    compare_exchange_weak(void*& __v1, void* __v2, memory_order __m1,
			  memory_order __m2)
    {
      __glibcxx_assert(__m2 != memory_order_release);
      __glibcxx_assert(__m2 != memory_order_acq_rel);
      __glibcxx_assert(__m2 <= __m1);
      return _ATOMIC_CMPEXCHNG_(this, &__v1, __v2, __m1);
    }

    bool
    compare_exchange_weak(void*& __v1, void* __v2, memory_order __m1,
			  memory_order __m2) volatile
    {
      __glibcxx_assert(__m2 != memory_order_release);
      __glibcxx_assert(__m2 != memory_order_acq_rel);
      __glibcxx_assert(__m2 <= __m1);
      return _ATOMIC_CMPEXCHNG_(this, &__v1, __v2, __m1);
    }

    bool
    compare_exchange_weak(void*& __v1, void* __v2,
			  memory_order __m = memory_order_seq_cst)
    {
      return compare_exchange_weak(__v1, __v2, __m,
				   __calculate_memory_order(__m));
    }

    bool
    compare_exchange_weak(void*& __v1, void* __v2,
			  memory_order __m = memory_order_seq_cst) volatile
    {
      return compare_exchange_weak(__v1, __v2, __m,
				   __calculate_memory_order(__m));
    }

    bool
    compare_exchange_weak(const void*& __v1, const void* __v2,
			  memory_order __m1, memory_order __m2)
    {
      __glibcxx_assert(__m2 != memory_order_release);
      __glibcxx_assert(__m2 != memory_order_acq_rel);
      __glibcxx_assert(__m2 <= __m1);
      return _ATOMIC_CMPEXCHNG_(this, &__v1, __v2, __m1);
    }

    bool
    compare_exchange_weak(const void*& __v1, const void* __v2,
			  memory_order __m1, memory_order __m2) volatile
    {
      __glibcxx_assert(__m2 != memory_order_release);
      __glibcxx_assert(__m2 != memory_order_acq_rel);
      __glibcxx_assert(__m2 <= __m1);
      return _ATOMIC_CMPEXCHNG_(this, &__v1, __v2, __m1);
    }

    bool
    compare_exchange_weak(const void*& __v1, const void* __v2,
			  memory_order __m = memory_order_seq_cst)
    {
      return compare_exchange_weak(__v1, __v2, __m,
				   __calculate_memory_order(__m));
    }

    bool
    compare_exchange_weak(const void*& __v1, const void* __v2,
			  memory_order __m = memory_order_seq_cst) volatile
    {
      return compare_exchange_weak(__v1, __v2, __m,
				   __calculate_memory_order(__m));
    }

    bool
    compare_exchange_strong(void*& __v1, void* __v2, memory_order __m1,
			    memory_order __m2)
    {
      __glibcxx_assert(__m2 != memory_order_release);
      __glibcxx_assert(__m2 != memory_order_acq_rel);
      __glibcxx_assert(__m2 <= __m1);
      return _ATOMIC_CMPEXCHNG_(this, &__v1, __v2, __m1);
    }

    bool
    compare_exchange_strong(void*& __v1, void* __v2, memory_order __m1,
			    memory_order __m2) volatile
    {
      __glibcxx_assert(__m2 != memory_order_release);
      __glibcxx_assert(__m2 != memory_order_acq_rel);
      __glibcxx_assert(__m2 <= __m1);
      return _ATOMIC_CMPEXCHNG_(this, &__v1, __v2, __m1);
    }

    bool
    compare_exchange_strong(void*& __v1, void* __v2,
			    memory_order __m = memory_order_seq_cst)
    {
      return compare_exchange_strong(__v1, __v2, __m,
				     __calculate_memory_order(__m));
    }

    bool
    compare_exchange_strong(void*& __v1, void* __v2,
			    memory_order __m = memory_order_seq_cst) volatile
    {
      return compare_exchange_strong(__v1, __v2, __m,
				     __calculate_memory_order(__m));
    }

    bool
    compare_exchange_strong(const void*& __v1, const void* __v2,
			    memory_order __m1, memory_order __m2)
    {
      __glibcxx_assert(__m2 != memory_order_release);
      __glibcxx_assert(__m2 != memory_order_acq_rel);
      __glibcxx_assert(__m2 <= __m1);
      return _ATOMIC_CMPEXCHNG_(this, &__v1, __v2, __m1);
    }

    bool
    compare_exchange_strong(const void*& __v1, const void* __v2,
			    memory_order __m1, memory_order __m2) volatile
    {
      __glibcxx_assert(__m2 != memory_order_release);
      __glibcxx_assert(__m2 != memory_order_acq_rel);
      __glibcxx_assert(__m2 <= __m1);
      return _ATOMIC_CMPEXCHNG_(this, &__v1, __v2, __m1);
    }

    bool
    compare_exchange_strong(const void*& __v1, const void* __v2,
			    memory_order __m = memory_order_seq_cst)
    {
      return compare_exchange_strong(__v1, __v2, __m,
				     __calculate_memory_order(__m));
    }

    bool
    compare_exchange_strong(const void*& __v1, const void* __v2,
			    memory_order __m = memory_order_seq_cst) volatile
    {
      return compare_exchange_strong(__v1, __v2, __m,
				     __calculate_memory_order(__m));
    }

    void*
    fetch_add(ptrdiff_t __d, memory_order __m = memory_order_seq_cst)
    {
      void** __p = &(_M_i);
      __atomic_flag_base* __g = __atomic_flag_for_address(__p);
      __atomic_flag_wait_explicit(__g, __m);
      void* __r = *__p;
      *__p = (void*)((char*)(*__p) + __d);
      atomic_flag_clear_explicit(__g, __m);
      return __r;
    }

    void*
    fetch_add(ptrdiff_t __d, memory_order __m = memory_order_seq_cst) volatile
    {
      void* volatile* __p = &(_M_i);
      __atomic_flag_base* __g = __atomic_flag_for_address(__p);
      __atomic_flag_wait_explicit(__g, __m);
      void* __r = *__p;
      *__p = (void*)((char*)(*__p) + __d);
      atomic_flag_clear_explicit(__g, __m);
      return __r;
    }

    void*
    fetch_sub(ptrdiff_t __d, memory_order __m = memory_order_seq_cst)
    {
      void** __p = &(_M_i);
      __atomic_flag_base* __g = __atomic_flag_for_address(__p);
      __atomic_flag_wait_explicit(__g, __m);
      void* __r = *__p;
      *__p = (void*)((char*)(*__p) - __d);
      atomic_flag_clear_explicit(__g, __m);
      return __r;
    }

    void*
    fetch_sub(ptrdiff_t __d, memory_order __m = memory_order_seq_cst) volatile
    {
      void* volatile* __p = &(_M_i);
      __atomic_flag_base* __g = __atomic_flag_for_address(__p);
      __atomic_flag_wait_explicit(__g, __m);
      void* __r = *__p;
      *__p = (void*)((char*)(*__p) - __d);
      atomic_flag_clear_explicit(__g, __m);
      return __r;
    }

    operator void*() const
    { return load(); }

    operator void*() const volatile
    { return load(); }

    // XXX
    void*
    operator=(void* __v)
    {
      store(__v);
      return __v;
    }

    void*
    operator=(void* __v) volatile
    {
      store(__v);
      return __v;
    }

    void*
    operator+=(ptrdiff_t __d)
    { return fetch_add(__d) + __d; }

    void*
    operator+=(ptrdiff_t __d) volatile
    { return fetch_add(__d) + __d; }

    void*
    operator-=(ptrdiff_t __d)
    { return fetch_sub(__d) - __d; }

    void*
    operator-=(ptrdiff_t __d) volatile
    { return fetch_sub(__d) - __d; }
  };


  /// Base class for atomic integrals.
  //
  // For each of the integral types, define atomic_[integral type] struct
  //
  // atomic_bool     bool
  // atomic_char     char
  // atomic_schar    signed char
  // atomic_uchar    unsigned char
  // atomic_short    short
  // atomic_ushort   unsigned short
  // atomic_int      int
  // atomic_uint     unsigned int
  // atomic_long     long
  // atomic_ulong    unsigned long
  // atomic_llong    long long
  // atomic_ullong   unsigned long long
  // atomic_char16_t char16_t
  // atomic_char32_t char32_t
  // atomic_wchar_t  wchar_t

  // Base type.
  // NB: Assuming _ITp is an integral scalar type that is 1, 2, 4, or 8 bytes,
  // since that is what GCC built-in functions for atomic memory access work on.
  template<typename _ITp>
    struct __atomic_base
    {
    private:
      typedef _ITp 	__int_type;

      __int_type 	_M_i;

    public:
      __atomic_base() = default;
      ~__atomic_base() = default;
      __atomic_base(const __atomic_base&) = delete;
      __atomic_base& operator=(const __atomic_base&) = delete;
      __atomic_base& operator=(const __atomic_base&) volatile = delete;

      // Requires __int_type convertible to _M_base._M_i.
      constexpr __atomic_base(__int_type __i): _M_i (__i) { }

      operator __int_type() const
      { return load(); }

      operator __int_type() const volatile
      { return load(); }

      __int_type
      operator=(__int_type __i)
      {
	store(__i);
	return __i;
      }

      __int_type
      operator=(__int_type __i) volatile
      {
	store(__i);
	return __i;
      }

      __int_type
      operator++(int)
      { return fetch_add(1); }

      __int_type
      operator++(int) volatile
      { return fetch_add(1); }

      __int_type
      operator--(int)
      { return fetch_sub(1); }

      __int_type
      operator--(int) volatile
      { return fetch_sub(1); }

      __int_type
      operator++()
      { return fetch_add(1) + 1; }

      __int_type
      operator++() volatile
      { return fetch_add(1) + 1; }

      __int_type
      operator--()
      { return fetch_sub(1) - 1; }

      __int_type
      operator--() volatile
      { return fetch_sub(1) - 1; }

      __int_type
      operator+=(__int_type __i)
      { return fetch_add(__i) + __i; }

      __int_type
      operator+=(__int_type __i) volatile
      { return fetch_add(__i) + __i; }

      __int_type
      operator-=(__int_type __i)
      { return fetch_sub(__i) - __i; }

      __int_type
      operator-=(__int_type __i) volatile
      { return fetch_sub(__i) - __i; }

      __int_type
      operator&=(__int_type __i)
      { return fetch_and(__i) & __i; }

      __int_type
      operator&=(__int_type __i) volatile
      { return fetch_and(__i) & __i; }

      __int_type
      operator|=(__int_type __i)
      { return fetch_or(__i) | __i; }

      __int_type
      operator|=(__int_type __i) volatile
      { return fetch_or(__i) | __i; }

      __int_type
      operator^=(__int_type __i)
      { return fetch_xor(__i) ^ __i; }

      __int_type
      operator^=(__int_type __i) volatile
      { return fetch_xor(__i) ^ __i; }

      bool
      is_lock_free() const
      { return false; }

      bool
      is_lock_free() const volatile
      { return false; }

      void
      store(__int_type __i, memory_order __m = memory_order_seq_cst)
      {
	__glibcxx_assert(__m != memory_order_acquire);
	__glibcxx_assert(__m != memory_order_acq_rel);
	__glibcxx_assert(__m != memory_order_consume);
	_ATOMIC_STORE_(this, __i, __m);
      }

      void
      store(__int_type __i, memory_order __m = memory_order_seq_cst) volatile
      {
	__glibcxx_assert(__m != memory_order_acquire);
	__glibcxx_assert(__m != memory_order_acq_rel);
	__glibcxx_assert(__m != memory_order_consume);
	_ATOMIC_STORE_(this, __i, __m);
      }

      __int_type
      load(memory_order __m = memory_order_seq_cst) const
      {
	__glibcxx_assert(__m != memory_order_release);
	__glibcxx_assert(__m != memory_order_acq_rel);
	return _ATOMIC_LOAD_(this, __m);
      }

      __int_type
      load(memory_order __m = memory_order_seq_cst) const volatile
      {
	__glibcxx_assert(__m != memory_order_release);
	__glibcxx_assert(__m != memory_order_acq_rel);
	return _ATOMIC_LOAD_(this, __m);
      }

      __int_type
      exchange(__int_type __i, memory_order __m = memory_order_seq_cst)
      { return _ATOMIC_MODIFY_(this, =, __i, __m); }

      __int_type
      exchange(__int_type __i, memory_order __m = memory_order_seq_cst) volatile
      { return _ATOMIC_MODIFY_(this, =, __i, __m); }

      bool
      compare_exchange_weak(__int_type& __i1, __int_type __i2,
			    memory_order __m1, memory_order __m2)
      {
	__glibcxx_assert(__m2 != memory_order_release);
	__glibcxx_assert(__m2 != memory_order_acq_rel);
	__glibcxx_assert(__m2 <= __m1);
	return _ATOMIC_CMPEXCHNG_(this, &__i1, __i2, __m1);
      }

      bool
      compare_exchange_weak(__int_type& __i1, __int_type __i2,
			    memory_order __m1, memory_order __m2) volatile
      {
	__glibcxx_assert(__m2 != memory_order_release);
	__glibcxx_assert(__m2 != memory_order_acq_rel);
	__glibcxx_assert(__m2 <= __m1);
	return _ATOMIC_CMPEXCHNG_(this, &__i1, __i2, __m1);
      }

      bool
      compare_exchange_weak(__int_type& __i1, __int_type __i2,
			    memory_order __m = memory_order_seq_cst)
      {
	return compare_exchange_weak(__i1, __i2, __m,
				     __calculate_memory_order(__m));
      }

      bool
      compare_exchange_weak(__int_type& __i1, __int_type __i2,
			    memory_order __m = memory_order_seq_cst) volatile
      {
	return compare_exchange_weak(__i1, __i2, __m,
				     __calculate_memory_order(__m));
      }

      bool
      compare_exchange_strong(__int_type& __i1, __int_type __i2,
			      memory_order __m1, memory_order __m2)
      {
	__glibcxx_assert(__m2 != memory_order_release);
	__glibcxx_assert(__m2 != memory_order_acq_rel);
	__glibcxx_assert(__m2 <= __m1);
	return _ATOMIC_CMPEXCHNG_(this, &__i1, __i2, __m1);
      }

      bool
      compare_exchange_strong(__int_type& __i1, __int_type __i2,
			      memory_order __m1, memory_order __m2) volatile
      {
	__glibcxx_assert(__m2 != memory_order_release);
	__glibcxx_assert(__m2 != memory_order_acq_rel);
	__glibcxx_assert(__m2 <= __m1);
	return _ATOMIC_CMPEXCHNG_(this, &__i1, __i2, __m1);
      }

      bool
      compare_exchange_strong(__int_type& __i1, __int_type __i2,
			      memory_order __m = memory_order_seq_cst)
      {
	return compare_exchange_strong(__i1, __i2, __m,
				       __calculate_memory_order(__m));
      }

      bool
      compare_exchange_strong(__int_type& __i1, __int_type __i2,
			      memory_order __m = memory_order_seq_cst) volatile
      {
	return compare_exchange_strong(__i1, __i2, __m,
				       __calculate_memory_order(__m));
      }

      __int_type
      fetch_add(__int_type __i, memory_order __m = memory_order_seq_cst)
      { return _ATOMIC_MODIFY_(this, +=, __i, __m); }

      __int_type
      fetch_add(__int_type __i,
		memory_order __m = memory_order_seq_cst) volatile
      { return _ATOMIC_MODIFY_(this, +=, __i, __m); }

      __int_type
      fetch_sub(__int_type __i, memory_order __m = memory_order_seq_cst)
      { return _ATOMIC_MODIFY_(this, -=, __i, __m); }

      __int_type
      fetch_sub(__int_type __i,
		memory_order __m = memory_order_seq_cst) volatile
      { return _ATOMIC_MODIFY_(this, -=, __i, __m); }

      __int_type
      fetch_and(__int_type __i, memory_order __m = memory_order_seq_cst)
      { return _ATOMIC_MODIFY_(this, &=, __i, __m); }

      __int_type
      fetch_and(__int_type __i,
		memory_order __m = memory_order_seq_cst) volatile
      { return _ATOMIC_MODIFY_(this, &=, __i, __m); }

      __int_type
      fetch_or(__int_type __i, memory_order __m = memory_order_seq_cst)
      { return _ATOMIC_MODIFY_(this, |=, __i, __m); }

      __int_type
      fetch_or(__int_type __i, memory_order __m = memory_order_seq_cst) volatile
      { return _ATOMIC_MODIFY_(this, |=, __i, __m); }

      __int_type
      fetch_xor(__int_type __i, memory_order __m = memory_order_seq_cst)
      { return _ATOMIC_MODIFY_(this, ^=, __i, __m); }

      __int_type
      fetch_xor(__int_type __i,
		memory_order __m = memory_order_seq_cst) volatile
      { return _ATOMIC_MODIFY_(this, ^=, __i, __m); }
    };

#undef _ATOMIC_LOAD_
#undef _ATOMIC_STORE_
#undef _ATOMIC_MODIFY_
#undef _ATOMIC_CMPEXCHNG_
} // namespace __atomic0

_GLIBCXX_END_NAMESPACE

#endif
