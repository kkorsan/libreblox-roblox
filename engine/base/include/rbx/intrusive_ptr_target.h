#pragma once

#include <cstdlib>
#include <boost/intrusive_ptr.hpp>
#include "rbx/Debug.h"
#include "rbx/atomic.h"
#include "rbx/Declarations.h"
#include "boost/cast.hpp"

/// Forward Declarations
namespace rbx
{
	/*
		quick_intrusive_ptr_target<> is a mix-in that implements all functions required
		to use boost::intrusive_ptr.
		maxRefs should be used if Count is a byte or short and there is the risk of an
		overflow. Some algorithms do not have this risk. To avoid writing less performant
		code, maxRefs is not strictly enforced. In a multithreaded environment you should
		pick a "reasonable" value that is less than std::numeric_limits<Count>::max. For
		example, if Count=byte, then maxRefs could be 240
	*/
	template<class T, typename Count, Count maxRefs>
	class quick_intrusive_ptr_target;

	/*
		intrusive_ptr_target<> is a mix-in that implements all functions required
		to use boost::intrusive_ptr and rbx::intrusive_weak_ptr. If you don't need
		weak reference support, then use quick_intrusive_ptr_target
		TODO: Allow custom allocators
	*/
	template<class T, typename Count, Count maxStrong, Count maxWeak>
	class intrusive_ptr_target;

    class too_many_refs : public std::exception
    {
    public:
        virtual const char* what() const throw()
        {
            return "too many refs";
        }
    };
}

/// For modern Boost compatibility (1.87+), we use ADL with friend functions

namespace rbx
{
#pragma pack(push)
#pragma pack(8)	// Packing is useful if Count is short or byte
	template<class T, typename Count = int, Count maxRefs = 0 >
	class RBXBaseClass quick_intrusive_ptr_target
	{
	public:
		rbx::atomic<Count> refs;
		inline quick_intrusive_ptr_target() { refs = 0; }

		friend void intrusive_ptr_add_ref(const quick_intrusive_ptr_target* p)
		{
			if (maxRefs > 0 && p->refs >= maxRefs)
				throw rbx::too_many_refs();
			const_cast<quick_intrusive_ptr_target*>(p)->refs++;
		}

		friend void intrusive_ptr_release(const quick_intrusive_ptr_target* p)
		{
			RBXASSERT(p->refs > 0);
			if (--(const_cast<quick_intrusive_ptr_target*>(p)->refs) == 0)
				delete static_cast<const T*>(p);
		}
	};
#pragma pack(pop)


	template<class T, typename Count = int, Count maxStrong = 0, Count maxWeak = maxStrong >
	class RBXBaseClass intrusive_ptr_target
	{
	private:
		// The "counts" struct is placed in memory at the head of the object
#pragma pack(push)
#pragma pack(8)	// Packing is useful if Count is short or byte
		struct counts
		{
			rbx::atomic<Count> strong;   // #shared
			rbx::atomic<Count> weak;     // #weak + (#shared != 0)
			counts()
			{
				strong = 0;
				weak = 1;
			}
		};
#pragma pack(pop)

		static inline counts* fetch(const T* t)
		{
			return reinterpret_cast<counts*>((char*) t - sizeof(counts));
		}
	public:

		void* operator new(std::size_t t)
		{
			void* c = ::malloc(sizeof(counts) + t);

			// placement new the counts:
			new (c) counts();

			return (char*)c + sizeof(counts);
		}

		void operator delete( void * p )
		{
			counts* c = fetch(reinterpret_cast<T*>(p));
			// operator delete should only be called if this object
			// never got touched by the intrusive_ptr functions
			RBXASSERT(c->strong == 0);
			RBXASSERT(c->weak == 1);
			::free(c);
		}

		friend void intrusive_ptr_add_ref(const intrusive_ptr_target* p)
		{
			counts* c = fetch(static_cast<const T*>(p));
			if (maxStrong > 0)
			{
				if (++(c->strong) > maxStrong)
					throw rbx::too_many_refs();
			}
			else
			{
				c->strong++;
				RBXASSERT(c->strong < std::numeric_limits<Count>::max() - 10);
			}
		}

		friend void intrusive_ptr_release(const intrusive_ptr_target* p)
		{
			const T* t = static_cast<const T*>(p);
			counts* c = fetch(t);
			if (--(c->strong) == 0)
			{
				t->~T();
				if (--(c->weak) == 0)
				{
					c->counts::~counts();
					::free((void*)c);
				}
			}
		}

		friend void intrusive_ptr_add_weak_ref(const intrusive_ptr_target* p)
		{
			counts* c = fetch(static_cast<const T*>(p));
			RBXASSERT(c->strong > 0);
			if (maxWeak > 0)
			{
				if (++(c->weak) > maxWeak + 1)
					throw rbx::too_many_refs();
			}
			else
			{
				++(c->weak);
				RBXASSERT(c->weak < std::numeric_limits<Count>::max() - 10);
			}
		}

		friend bool intrusive_ptr_expired(const intrusive_ptr_target* p)
		{
			counts* c = fetch(static_cast<const T*>(p));
			return c->strong == 0;
		}

		friend bool intrusive_ptr_try_lock(const intrusive_ptr_target* p)
		{
			counts* c = fetch(static_cast<const T*>(p));
			while (true)
			{
				Count tmp = c->strong;
				if (tmp == 0)
					return false;
				if (c->strong.compare_and_swap(tmp + 1, tmp) == tmp)
					return true;
			}
		}

		friend void intrusive_ptr_weak_release(const intrusive_ptr_target* p)
		{
			const T* t = static_cast<const T*>(p);
			counts* c = fetch(t);
			if (--(c->weak) == 0)
			{
				RBXASSERT(c->strong == 0);
				c->counts::~counts();
				::free((void*)c);
			}
		}
	};
}

// Add boost namespace wrapper functions for compatibility with existing code that calls boost::intrusive_ptr_expired etc.
namespace boost
{


	template<class T, typename Count, Count maxStrong, Count maxWeak>
	void intrusive_ptr_add_weak_ref(const rbx::intrusive_ptr_target<T, Count, maxStrong, maxWeak>* p)
	{
		intrusive_ptr_add_weak_ref(p); // Calls friend function via ADL
	}

	template<class T, typename Count, Count maxStrong, Count maxWeak>
	bool intrusive_ptr_expired(const rbx::intrusive_ptr_target<T, Count, maxStrong, maxWeak>* p)
	{
		return intrusive_ptr_expired(p); // Calls friend function via ADL
	}

	template<class T, typename Count, Count maxStrong, Count maxWeak>
	bool intrusive_ptr_try_lock(const rbx::intrusive_ptr_target<T, Count, maxStrong, maxWeak>* p)
	{
		return intrusive_ptr_try_lock(p); // Calls friend function via ADL
	}

	template<class T, typename Count, Count maxStrong, Count maxWeak>
	void intrusive_ptr_weak_release(const rbx::intrusive_ptr_target<T, Count, maxStrong, maxWeak>* p)
	{
		intrusive_ptr_weak_release(p); // Calls friend function via ADL
	}
}
