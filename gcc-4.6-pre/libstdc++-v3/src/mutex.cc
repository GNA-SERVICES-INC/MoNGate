// mutex -*- C++ -*-

// Copyright (C) 2008, 2009, 2010 Free Software Foundation, Inc.
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

#include <mutex>

#if defined(_GLIBCXX_HAS_GTHREADS) && defined(_GLIBCXX_USE_C99_STDINT_TR1)
#ifndef _GLIBCXX_HAVE_TLS
namespace
{
  inline std::unique_lock<std::mutex>*&
  __get_once_functor_lock_ptr()
  {
    static std::unique_lock<std::mutex>* __once_functor_lock_ptr = 0;
    return __once_functor_lock_ptr;
  }
}
#endif

_GLIBCXX_BEGIN_NAMESPACE(std)

#ifdef _GLIBCXX_HAVE_TLS
  __thread void* __once_callable;
  __thread void (*__once_call)();
#else
  // Explicit instantiation due to -fno-implicit-instantiation.
  template class function<void()>;
  function<void()> __once_functor;

  mutex&
  __get_once_mutex()
  {
    static mutex once_mutex;
    return once_mutex;
  }

  // code linked against ABI 3.4.12 and later uses this
  void
  __set_once_functor_lock_ptr(unique_lock<mutex>* __ptr)
  {
    __get_once_functor_lock_ptr() = __ptr;
  }

  // unsafe - retained for compatibility with ABI 3.4.11
  unique_lock<mutex>&
  __get_once_functor_lock()
  {
    static unique_lock<mutex> once_functor_lock(__get_once_mutex(), defer_lock);
    return once_functor_lock;
  }
#endif

  extern "C"
  {
    void __once_proxy()
    {
#ifndef _GLIBCXX_HAVE_TLS
      function<void()> __once_call = std::move(__once_functor);
      if (unique_lock<mutex>* __lock = __get_once_functor_lock_ptr())
      {
        // caller is using new ABI and provided lock ptr
        __get_once_functor_lock_ptr() = 0;
        __lock->unlock();
      }
      else
        __get_once_functor_lock().unlock();  // global lock
#endif
      __once_call();
    }
  }

_GLIBCXX_END_NAMESPACE

// XXX GLIBCXX_ABI Deprecated
// gcc-4.6.0
// <mutex> export changes
#if defined(_GLIBCXX_SYMVER_GNU) && defined(PIC) \
    && defined(_GLIBCXX_HAVE_AS_SYMVER_DIRECTIVE) \
    && defined(_GLIBCXX_HAVE_SYMVER_SYMBOL_RENAMING_RUNTIME_SUPPORT)

namespace __gnu_cxx
{
  std::defer_lock_t defer_lock;
  std::try_to_lock_t try_to_lock;
  std::adopt_lock_t adopt_lock;
}

#define _GLIBCXX_ASM_SYMVER(cur, old, version) \
   asm (".symver " #cur "," #old "@@" #version);

_GLIBCXX_ASM_SYMVER(_ZN9__gnu_cxx10adopt_lockE, _ZSt10adopt_lock, GLIBCXX_3.4.11)
_GLIBCXX_ASM_SYMVER(_ZN9__gnu_cxx10defer_lockE, _ZSt10defer_lock, GLIBCXX_3.4.11)
_GLIBCXX_ASM_SYMVER(_ZN9__gnu_cxx11try_to_lockE, _ZSt11try_to_lock, GLIBCXX_3.4.11)


#endif

#endif // _GLIBCXX_HAS_GTHREADS && _GLIBCXX_USE_C99_STDINT_TR1