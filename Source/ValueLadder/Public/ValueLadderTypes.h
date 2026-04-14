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
		inline int32 ResolveLadderIndexFromVerticalOffset(const int32 StartIndex, const double VerticalOffsetPx, const double RowHeightPx, const int32 MinIndex, const int32 MaxIndex)
		{
			const double SafeRowHeightPx = FMath::Max(RowHeightPx, KINDA_SMALL_NUMBER);
			const int32 LadderIndexOffset = FMath::RoundToInt(VerticalOffsetPx / SafeRowHeightPx);
			return FMath::Clamp(StartIndex + LadderIndexOffset, MinIndex, MaxIndex);
		}

		inline double GetSafeThresholdPx(const double ThresholdPx)
		{
			return FMath::Max(ThresholdPx, KINDA_SMALL_NUMBER);
		}

		inline int32 ComputeBucketCount(const double PixelDelta, const double ThresholdPx)
		{
			const double SafeThresholdPx = GetSafeThresholdPx(ThresholdPx);
			return FMath::TruncToInt(PixelDelta / SafeThresholdPx);
		}

		inline double ComputeTickRemainderPx(const double PixelDelta, const double ThresholdPx)
		{
			const double SafeThresholdPx = GetSafeThresholdPx(ThresholdPx);
			return PixelDelta - static_cast<double>(ComputeBucketCount(PixelDelta, SafeThresholdPx)) * SafeThresholdPx;
		}

		inline double ComputeTickProgress(const double PixelDelta, const double ThresholdPx)
		{
			const double SafeThresholdPx = GetSafeThresholdPx(ThresholdPx);
			return FMath::Clamp(FMath::Abs(ComputeTickRemainderPx(PixelDelta, SafeThresholdPx)) / SafeThresholdPx, 0.0, 1.0);
		}

		inline double ComputePixelsToNextTick(const double PixelDelta, const double ThresholdPx)
		{
			const double SafeThresholdPx = GetSafeThresholdPx(ThresholdPx);
			const double RemainderPx = FMath::Abs(ComputeTickRemainderPx(PixelDelta, SafeThresholdPx));
			return FMath::IsNearlyZero(RemainderPx) ? SafeThresholdPx : SafeThresholdPx - RemainderPx;
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
