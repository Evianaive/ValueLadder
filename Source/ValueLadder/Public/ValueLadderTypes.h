#pragma once

#include "CoreMinimal.h"

enum class EValueLadderNumericType : uint8
{
	Float,
	Double,
	Int32
};

struct FValueLadderConstraintRange
{
	TOptional<double> MinValue;
	TOptional<double> MaxValue;

	double Clamp(const double InValue) const
	{
		double ClampedValue = InValue;
		if (MinValue.IsSet())
		{
			ClampedValue = FMath::Max(ClampedValue, MinValue.GetValue());
		}

		if (MaxValue.IsSet())
		{
			ClampedValue = FMath::Min(ClampedValue, MaxValue.GetValue());
		}

		return ClampedValue;
	}
};

namespace ValueLadder::Math
{
	inline double ComputeDelta(const double PixelDelta, const double Sensitivity, const double StepMultiplier)
	{
		return PixelDelta * Sensitivity * StepMultiplier;
	}

	inline double ApplyIntegerRounding(const double InValue)
	{
		return FMath::RoundToDouble(InValue);
	}
}
