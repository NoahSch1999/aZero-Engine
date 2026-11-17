#pragma once
#include <utility>

namespace aZero
{
	template<typename T>
	class RefCounted
	{
	public:
		RefCounted() = default;
		RefCounted(T& ref)
			:m_Reference(&ref)
		{
			m_Reference->AddRef();
		}

		RefCounted(RefCounted&& other)
		{
			std::swap(m_Reference, other.m_Reference);
		}

		RefCounted& operator=(RefCounted&& other)
		{
			std::swap(m_Reference, other.m_Reference);
			return *this;
		}

		RefCounted(const RefCounted& other)
		{
			m_Reference = other.m_Reference;
			if (m_Reference)
			{
				m_Reference->AddRef();
			}
		}

		RefCounted& operator=(const RefCounted& other)
		{
			m_Reference = other.m_Reference;
			if (m_Reference)
			{
				m_Reference->AddRef();
			}
			return *this;
		}

		~RefCounted()
		{
			if(m_Reference)
			{
				m_Reference->RemoveRef();
			}
		}

	private:
		T* m_Reference = nullptr;
	};
}