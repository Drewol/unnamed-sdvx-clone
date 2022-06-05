#include <Shared/Shared.hpp>
#include <Shared/Enum.hpp>
#include <Tests/Tests.hpp>

DefineEnum(TestEnum,
	A = 0x0,
	B = 0x2,
	C,
	D,
	OO);

DefineBitflagEnum(TestFlags,
	Color = 0x1,
	Float = 0x2,
	Transparent = 0x4);
ImplementBitflagEnum(TestFlags);

Test("Enum.String.Regular")
{
	TestEnsure(Enum_TestEnum::ToString(TestEnum::A) == "A");
	TestEnsure(Enum_TestEnum::FromString("A") == TestEnum::A);
	TestEnsure(Enum_TestEnum::FromString("C") == TestEnum::C);
}
Test("Enum.String.Bitflags")
{
	TestEnsure(Enum_TestFlags::ToString(TestFlags::Color | TestFlags::Float) == "Color | Float");
	TestEnsure(Enum_TestFlags::FromString("Float") == TestFlags::Float);
	TestEnsure(Enum_TestFlags::FromString("Transparent | Float") == (TestFlags::Float | TestFlags::Transparent));
}
