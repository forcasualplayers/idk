#pragma once
#include <core/ISystem.h>

namespace idk
{
	class TestSystem : public ISystem
	{
	public:
		void Init() override;
		void TestSpan(span<class TestComponent>);
		void Shutdown() override;
	};
}