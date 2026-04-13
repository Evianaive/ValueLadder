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
