#include "ValueLadderSettings.h"

namespace
{
	FValueLadderUnitOverride MakeUnitOverride(
		const FName Unit,
		const TArray<float>& FloatLadders,
		const int32 DefaultFloatLadderIndex,
		const TArray<int32>& IntLadders,
		const int32 DefaultIntLadderIndex)
	{
		FValueLadderUnitOverride UnitOverride;
		UnitOverride.Unit = Unit;
		UnitOverride.FloatLadders = FloatLadders;
		UnitOverride.DefaultFloatLadderIndex = DefaultFloatLadderIndex;
		UnitOverride.IntLadders = IntLadders;
		UnitOverride.DefaultIntLadderIndex = DefaultIntLadderIndex;
		return UnitOverride;
	}

	FValueLadderSemanticOverride MakeSemanticOverride(
		const EValueLadderSemanticRole SemanticRole,
		const FName Unit,
		const TArray<float>& FloatLadders,
		const int32 DefaultFloatLadderIndex,
		const TArray<int32>& IntLadders,
		const int32 DefaultIntLadderIndex)
	{
		FValueLadderSemanticOverride SemanticOverride;
		SemanticOverride.SemanticRole = SemanticRole;
		SemanticOverride.Unit = Unit;
		SemanticOverride.FloatLadders = FloatLadders;
		SemanticOverride.DefaultFloatLadderIndex = DefaultFloatLadderIndex;
		SemanticOverride.IntLadders = IntLadders;
		SemanticOverride.DefaultIntLadderIndex = DefaultIntLadderIndex;
		return SemanticOverride;
	}

	FString NormalizeUnitString(const FName UnitKey)
	{
		if (UnitKey.IsNone())
		{
			return FString();
		}

		FString NormalizedUnit = UnitKey.ToString().TrimStartAndEnd();
		if (NormalizedUnit.Equals(TEXT("deg"), ESearchCase::IgnoreCase))
		{
			return TEXT("Degrees");
		}
		if (NormalizedUnit.Equals(TEXT("cm"), ESearchCase::IgnoreCase))
		{
			return TEXT("Centimeters");
		}
		if (NormalizedUnit.Equals(TEXT("m"), ESearchCase::IgnoreCase))
		{
			return TEXT("Meters");
		}
		if (NormalizedUnit.Equals(TEXT("s"), ESearchCase::IgnoreCase))
		{
			return TEXT("Seconds");
		}
		if (NormalizedUnit.Equals(TEXT("ms"), ESearchCase::IgnoreCase))
		{
			return TEXT("Milliseconds");
		}
		if (NormalizedUnit.Equals(TEXT("kg"), ESearchCase::IgnoreCase))
		{
			return TEXT("Kilograms");
		}

		return NormalizedUnit;
	}

	const FValueLadderUnitOverride* FindUnitOverride(const UValueLadderSettings& Settings, const FName UnitKey)
	{
		const FString NormalizedUnit = NormalizeUnitString(UnitKey);
		if (NormalizedUnit.IsEmpty())
		{
			return nullptr;
		}

		for (int32 OverrideIndex = Settings.UnitOverrides.Num() - 1; OverrideIndex >= 0; --OverrideIndex)
		{
			const FValueLadderUnitOverride& UnitOverride = Settings.UnitOverrides[OverrideIndex];
			const FString NormalizedOverrideUnit = NormalizeUnitString(UnitOverride.Unit);
			if (!NormalizedOverrideUnit.IsEmpty() && NormalizedOverrideUnit.Equals(NormalizedUnit, ESearchCase::IgnoreCase))
			{
				return &UnitOverride;
			}
		}

		return nullptr;
	}

	const FValueLadderSemanticOverride* FindSemanticOverride(const UValueLadderSettings& Settings, const EValueLadderSemanticRole SemanticRole, const FName UnitKey)
	{
		const FString NormalizedUnit = NormalizeUnitString(UnitKey);

		if (!NormalizedUnit.IsEmpty())
		{
			for (int32 OverrideIndex = Settings.SemanticOverrides.Num() - 1; OverrideIndex >= 0; --OverrideIndex)
			{
				const FValueLadderSemanticOverride& SemanticOverride = Settings.SemanticOverrides[OverrideIndex];
				if (SemanticOverride.SemanticRole != SemanticRole)
				{
					continue;
				}

				const FString NormalizedOverrideUnit = NormalizeUnitString(SemanticOverride.Unit);
				if (!NormalizedOverrideUnit.IsEmpty() && NormalizedOverrideUnit.Equals(NormalizedUnit, ESearchCase::IgnoreCase))
				{
					return &SemanticOverride;
				}
			}
		}

		for (int32 OverrideIndex = Settings.SemanticOverrides.Num() - 1; OverrideIndex >= 0; --OverrideIndex)
		{
			const FValueLadderSemanticOverride& SemanticOverride = Settings.SemanticOverrides[OverrideIndex];
			if (SemanticOverride.SemanticRole != SemanticRole)
			{
				continue;
			}

			if (NormalizeUnitString(SemanticOverride.Unit).IsEmpty())
			{
				return &SemanticOverride;
			}
		}

		return nullptr;
	}

	const TArray<float>& ResolveFloatLadders(const UValueLadderSettings& Settings, const FName UnitKey, const EValueLadderSemanticRole SemanticRole, int32& OutDefaultIndex)
	{
		if (const FValueLadderSemanticOverride* SemanticOverride = FindSemanticOverride(Settings, SemanticRole, UnitKey))
		{
			if (!SemanticOverride->FloatLadders.IsEmpty())
			{
				OutDefaultIndex = SemanticOverride->DefaultFloatLadderIndex;
				return SemanticOverride->FloatLadders;
			}
		}

		if (const FValueLadderUnitOverride* UnitOverride = FindUnitOverride(Settings, UnitKey))
		{
			if (!UnitOverride->FloatLadders.IsEmpty())
			{
				OutDefaultIndex = UnitOverride->DefaultFloatLadderIndex;
				return UnitOverride->FloatLadders;
			}
		}

		OutDefaultIndex = Settings.DefaultFloatLadderIndex;
		return Settings.FloatLadders;
	}

	const TArray<int32>& ResolveIntLadders(const UValueLadderSettings& Settings, const FName UnitKey, const EValueLadderSemanticRole SemanticRole, int32& OutDefaultIndex)
	{
		if (const FValueLadderSemanticOverride* SemanticOverride = FindSemanticOverride(Settings, SemanticRole, UnitKey))
		{
			if (!SemanticOverride->IntLadders.IsEmpty())
			{
				OutDefaultIndex = SemanticOverride->DefaultIntLadderIndex;
				return SemanticOverride->IntLadders;
			}
		}

		if (const FValueLadderUnitOverride* UnitOverride = FindUnitOverride(Settings, UnitKey))
		{
			if (!UnitOverride->IntLadders.IsEmpty())
			{
				OutDefaultIndex = UnitOverride->DefaultIntLadderIndex;
				return UnitOverride->IntLadders;
			}
		}

		OutDefaultIndex = Settings.DefaultIntLadderIndex;
		return Settings.IntLadders;
	}
}

UValueLadderSettings::UValueLadderSettings()
{
	if (UnitOverrides.IsEmpty())
	{
		const TArray<float> DegreeFloatLadders = {0.1f, 1.0f, 5.0f, 15.0f, 45.0f, 90.0f};
		const TArray<int32> DegreeIntLadders = {1, 5, 15, 45, 90, 180};
		const TArray<float> CentimeterFloatLadders = {0.1f, 1.0f, 10.0f, 100.0f, 1000.0f, 10000.0f};
		const TArray<int32> CentimeterIntLadders = {1, 10, 100, 1000, 10000};
		const TArray<float> MeterFloatLadders = {0.001f, 0.01f, 0.1f, 1.0f, 10.0f, 100.0f};
		const TArray<int32> MeterIntLadders = {1, 10, 100, 1000};
		const TArray<float> SecondFloatLadders = {0.001f, 0.01f, 0.1f, 1.0f, 10.0f, 60.0f};
		const TArray<int32> SecondIntLadders = {1, 5, 10, 30, 60, 300};
		const TArray<float> MillisecondFloatLadders = {0.1f, 1.0f, 5.0f, 10.0f, 100.0f, 1000.0f};
		const TArray<int32> MillisecondIntLadders = {1, 5, 10, 50, 100, 1000};
		const TArray<float> KilogramFloatLadders = {0.01f, 0.1f, 1.0f, 10.0f, 100.0f, 1000.0f};
		const TArray<int32> KilogramIntLadders = {1, 5, 10, 50, 100, 1000};
		const TArray<float> ScaleFloatLadders = {0.01f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f};
		const TArray<float> RotationFloatLadders = {0.1f, 1.0f, 5.0f, 15.0f, 45.0f, 90.0f};

		UnitOverrides = {
			MakeUnitOverride(TEXT("Degrees"), DegreeFloatLadders, 3, DegreeIntLadders, 2),
			MakeUnitOverride(TEXT("Centimeters"), CentimeterFloatLadders, 3, CentimeterIntLadders, 2),
			MakeUnitOverride(TEXT("Meters"), MeterFloatLadders, 3, MeterIntLadders, 0),
			MakeUnitOverride(TEXT("Seconds"), SecondFloatLadders, 3, SecondIntLadders, 0),
			MakeUnitOverride(TEXT("Milliseconds"), MillisecondFloatLadders, 3, MillisecondIntLadders, 2),
			MakeUnitOverride(TEXT("Kilograms"), KilogramFloatLadders, 2, KilogramIntLadders, 1)
		};

		SemanticOverrides = {
			MakeSemanticOverride(EValueLadderSemanticRole::Scale, NAME_None, ScaleFloatLadders, 4, {}, 0),
			MakeSemanticOverride(EValueLadderSemanticRole::Rotation, NAME_None, RotationFloatLadders, 3, {}, 0)
		};
	}
}

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
	case EValueLadderNumericType::Int8:
	case EValueLadderNumericType::UInt8:
	case EValueLadderNumericType::Int16:
	case EValueLadderNumericType::UInt16:
	case EValueLadderNumericType::Int32:
	case EValueLadderNumericType::UInt32:
	case EValueLadderNumericType::Int64:
	case EValueLadderNumericType::UInt64:
		return bEnableInt32;
	default:
		return false;
	}
}

int32 UValueLadderSettings::GetDefaultLadderIndex(const EValueLadderNumericType NumericType, const FName UnitKey, const EValueLadderSemanticRole SemanticRole) const
{
	switch (NumericType)
	{
	case EValueLadderNumericType::Float:
	case EValueLadderNumericType::Double:
		{
			int32 DefaultIndex = DefaultFloatLadderIndex;
			ResolveFloatLadders(*this, UnitKey, SemanticRole, DefaultIndex);
			return ClampLadderIndex(NumericType, DefaultIndex, UnitKey, SemanticRole);
		}
	case EValueLadderNumericType::Int8:
	case EValueLadderNumericType::UInt8:
	case EValueLadderNumericType::Int16:
	case EValueLadderNumericType::UInt16:
	case EValueLadderNumericType::Int32:
	case EValueLadderNumericType::UInt32:
	case EValueLadderNumericType::Int64:
	case EValueLadderNumericType::UInt64:
		{
			int32 DefaultIndex = DefaultIntLadderIndex;
			ResolveIntLadders(*this, UnitKey, SemanticRole, DefaultIndex);
			return ClampLadderIndex(NumericType, DefaultIndex, UnitKey, SemanticRole);
		}
	default:
		return 0;
	}
}

int32 UValueLadderSettings::ClampLadderIndex(const EValueLadderNumericType NumericType, const int32 Index, const FName UnitKey, const EValueLadderSemanticRole SemanticRole) const
{
	int32 DefaultIndex = 0;
	const int32 MaxIndex = ValueLadder::IsIntegerNumericType(NumericType)
		? ResolveIntLadders(*this, UnitKey, SemanticRole, DefaultIndex).Num() - 1
		: ResolveFloatLadders(*this, UnitKey, SemanticRole, DefaultIndex).Num() - 1;

	if (MaxIndex < 0)
	{
		return 0;
	}

	return FMath::Clamp(Index, 0, MaxIndex);
}

double UValueLadderSettings::GetLadderStep(const EValueLadderNumericType NumericType, const int32 Index, const FName UnitKey, const EValueLadderSemanticRole SemanticRole) const
{
	if (ValueLadder::IsIntegerNumericType(NumericType))
	{
		int32 DefaultIndex = DefaultIntLadderIndex;
		const TArray<int32>& Ladders = ResolveIntLadders(*this, UnitKey, SemanticRole, DefaultIndex);
		if (Ladders.IsEmpty())
		{
			return 1.0;
		}

		return static_cast<double>(Ladders[ClampLadderIndex(NumericType, Index, UnitKey, SemanticRole)]);
	}

	int32 DefaultIndex = DefaultFloatLadderIndex;
	const TArray<float>& Ladders = ResolveFloatLadders(*this, UnitKey, SemanticRole, DefaultIndex);
	if (Ladders.IsEmpty())
	{
		return 1.0;
	}

	return static_cast<double>(Ladders[ClampLadderIndex(NumericType, Index, UnitKey, SemanticRole)]);
}

void UValueLadderSettings::BuildLadderDisplayValues(const EValueLadderNumericType NumericType, TArray<FText>& OutValues, const FName UnitKey, const EValueLadderSemanticRole SemanticRole) const
{
	OutValues.Reset();

	if (ValueLadder::IsIntegerNumericType(NumericType))
	{
		int32 DefaultIndex = DefaultIntLadderIndex;
		const TArray<int32>& Ladders = ResolveIntLadders(*this, UnitKey, SemanticRole, DefaultIndex);
		OutValues.Reserve(Ladders.Num());
		for (const int32 LadderValue : Ladders)
		{
			OutValues.Add(FText::FromString(FString::FromInt(LadderValue)));
		}
		return;
	}

	int32 DefaultIndex = DefaultFloatLadderIndex;
	const TArray<float>& Ladders = ResolveFloatLadders(*this, UnitKey, SemanticRole, DefaultIndex);
	OutValues.Reserve(Ladders.Num());
	for (const float LadderValue : Ladders)
	{
		OutValues.Add(FText::FromString(FString::SanitizeFloat(LadderValue)));
	}
}

