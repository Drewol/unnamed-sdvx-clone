#pragma once
#include "Shared/Enum.hpp"

// In case None is defined by someone (linux)
#ifdef None
#undef None
#endif

namespace Graphics
{
	DefineEnum(MouseButton,
		Left = 0,
		Middle,
		Right)

		DefineBitflagEnum(ModifierKeys,
			None = 0,
			Alt = 1,
			Ctrl = 2,
			Shift = 4)

		DefineEnum(Key,
			None = 0,
			// Top Row Keys
			Escape,
			F1,
			F2,
			F3,
			F4,
			F5,
			F6,
			F7,
			F8,
			F9,
			F10,
			F11,
			F12,
			PrntScr,
			ScrollLock,
			Pause,
			// Alpha keys
			A = 'A',
			B,
			C,
			D,
			E,
			F,
			G,
			H,
			I,
			J,
			K,
			L,
			M,
			N,
			O,
			P,
			Q,
			R,
			S,
			T,
			U,
			V,
			W,
			X,
			Y,
			Z,
			// Top row number keys
			Tilde,
			Top0,
			Top1,
			Top2,
			Top3,
			Top4,
			Top5,
			Top6,
			Top7,
			Top8,
			Top9,
			Minus,
			Plus,
			Backspace,
			// Arrow keys
			ArrowLeft,
			ArrowUp,
			ArrowRight,
			ArrowDown,
			// Other keys
			Space,
			Return,
			PageUp,
			PageDown,
			Tab)
}
