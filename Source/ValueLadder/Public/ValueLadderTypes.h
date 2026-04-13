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
		inline double ComputeSelectionColumnHalfWidth(const double ColumnWidthPx)
		{
			return FMath::Max(ColumnWidthPx * 0.5, 0.0);
		}

		inline bool IsInsideSelectionColumn(const double PixelOffset, const double ColumnWidthPx)
		{
			return FMath::Abs(PixelOffset) <= ComputeSelectionColumnHalfWidth(ColumnWidthPx);
		}

		inline double ApplySelectionColumnGate(const double PixelOffset, const double ColumnWidthPx)
		{
			const double HalfWidthPx = ComputeSelectionColumnHalfWidth(ColumnWidthPx);
			const double AbsoluteOffset = FMath::Abs(PixelOffset);
			if (AbsoluteOffset <= HalfWidthPx)
			{
				return 0.0;
			}

			const double SignedDirection = PixelOffset < 0.0 ? -1.0 : 1.0;
			return SignedDirection * (AbsoluteOffset - HalfWidthPx);
		}

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
