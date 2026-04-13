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
		inline int32 ComputeBucketCount(const double PixelDelta, const double ThresholdPx)
		{
			const double SafeThresholdPx = FMath::Max(ThresholdPx, KINDA_SMALL_NUMBER);
			return FMath::TruncToInt(PixelDelta / SafeThresholdPx);
		}

		inline double ComputeDelta(const double PixelDelta, const double ThresholdPx, const double LadderStep, const double StepMultiplier)
		{
			return LadderStep * static_cast<double>(ComputeBucketCount(PixelDelta, ThresholdPx)) * StepMultiplier;
		}

		inline double ComputeSegmentedDelta(
			const double BaseDelta,
			const double PixelOffset,
			const double SegmentStartPixelOffset,
			const double ThresholdPx,
			const double LadderStep,
			const double StepMultiplier)
		{
			return BaseDelta + ComputeDelta(PixelOffset - SegmentStartPixelOffset, ThresholdPx, LadderStep, StepMultiplier);
		}

		inline double ApplyIntegerRounding(const double InValue)
		{
			return FMath::RoundToDouble(InValue);
		}
}
