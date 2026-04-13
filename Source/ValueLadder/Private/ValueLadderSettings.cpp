#include "ValueLadderSettings.h"

double UValueLadderSettings::ResolveStepMultiplier(const bool bShiftDown, const bool bCtrlDown) const
{
	if (bShiftDown && !bCtrlDown)
	{
		return ShiftStepMultiplier;
	}

	if (bCtrlDown && !bShiftDown)
	{
		return CtrlStepMultiplier;
	}

	return 1.0;
}

double UValueLadderSettings::ComputeDeltaFromPixelOffset(const double PixelOffset, const double LadderStep, const bool bShiftDown, const bool bCtrlDown) const
{
	return ValueLadder::Math::ComputeDelta(PixelOffset, DragActivationThresholdPx, LadderStep, ResolveStepMultiplier(bShiftDown, bCtrlDown));
}

bool UValueLadderSettings::SupportsType(const EValueLadderNumericType NumericType) const
{
	switch (NumericType)
	{
	case EValueLadderNumericType::Float:
		return bEnableFloat;
	case EValueLadderNumericType::Double:
		return bEnableDouble;
	case EValueLadderNumericType::Int32:
		return bEnableInt32;
	default:
		return false;
	}
}

int32 UValueLadderSettings::GetDefaultLadderIndex(const EValueLadderNumericType NumericType) const
{
	switch (NumericType)
	{
	case EValueLadderNumericType::Float:
	case EValueLadderNumericType::Double:
		return ClampLadderIndex(NumericType, DefaultFloatLadderIndex);
	case EValueLadderNumericType::Int32:
		return ClampLadderIndex(NumericType, DefaultIntLadderIndex);
	default:
		return 0;
	}
}

int32 UValueLadderSettings::ClampLadderIndex(const EValueLadderNumericType NumericType, const int32 Index) const
{
	const int32 MaxIndex = NumericType == EValueLadderNumericType::Int32
		? IntLadders.Num() - 1
		: FloatLadders.Num() - 1;

	if (MaxIndex < 0)
	{
		return 0;
	}

	return FMath::Clamp(Index, 0, MaxIndex);
}

double UValueLadderSettings::GetLadderStep(const EValueLadderNumericType NumericType, const int32 Index) const
{
	if (NumericType == EValueLadderNumericType::Int32)
	{
		if (IntLadders.IsEmpty())
		{
			return 1.0;
		}

		return static_cast<double>(IntLadders[ClampLadderIndex(NumericType, Index)]);
	}

	if (FloatLadders.IsEmpty())
	{
		return 1.0;
	}

	return static_cast<double>(FloatLadders[ClampLadderIndex(NumericType, Index)]);
}

void UValueLadderSettings::BuildLadderDisplayValues(const EValueLadderNumericType NumericType, TArray<FText>& OutValues) const
{
	OutValues.Reset();

	if (NumericType == EValueLadderNumericType::Int32)
	{
		OutValues.Reserve(IntLadders.Num());
		for (const int32 LadderValue : IntLadders)
		{
			OutValues.Add(FText::FromString(FString::FromInt(LadderValue)));
		}
		return;
	}

	OutValues.Reserve(FloatLadders.Num());
	for (const float LadderValue : FloatLadders)
	{
		OutValues.Add(FText::FromString(FString::SanitizeFloat(LadderValue)));
	}
}

