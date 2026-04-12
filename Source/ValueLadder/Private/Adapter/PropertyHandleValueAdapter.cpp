#include "Adapter/PropertyHandleValueAdapter.h"

#include "UObject/UnrealType.h"

namespace
{
const FName NAME_ClampMin(TEXT("ClampMin"));
const FName NAME_ClampMax(TEXT("ClampMax"));
const FName NAME_UIMin(TEXT("UIMin"));
const FName NAME_UIMax(TEXT("UIMax"));
}

bool FPropertyHandleValueAdapter::CaptureBaseline(
	const FValueLadderPropertyTarget& Target,
	FValueLadderBaselineData& OutBaseline,
	FString& OutError) const
{
	if (!Target.IsValid())
	{
		OutError = TEXT("Property target is invalid.");
		return false;
	}

	if (!Target.PropertyHandle->IsEditable() || Target.PropertyHandle->IsEditConst())
	{
		OutError = TEXT("Property is not editable.");
		return false;
	}

	TArray<void*> RawData;
	Target.PropertyHandle->AccessRawData(RawData);
	if (RawData.IsEmpty())
	{
		OutError = TEXT("No raw property data found for current selection.");
		return false;
	}

	OutBaseline.BaselineValues.Reset();
	OutBaseline.BaselineValues.Reserve(RawData.Num());
	for (void* RawPtr : RawData)
	{
		if (RawPtr == nullptr)
		{
			continue;
		}

		OutBaseline.BaselineValues.Add(ReadScalar(RawPtr, Target.NumericType));
	}

	if (OutBaseline.BaselineValues.IsEmpty())
	{
		OutError = TEXT("Failed to capture baseline values.");
		return false;
	}

	PopulateConstraints(Target.PropertyHandle, OutBaseline.Constraints);
	return true;
}

bool FPropertyHandleValueAdapter::ApplyDelta(
	const FValueLadderPropertyTarget& Target,
	const FValueLadderBaselineData& Baseline,
	const double Delta,
	const bool bInteractive) const
{
	if (!Target.IsValid() || Baseline.BaselineValues.IsEmpty())
	{
		return false;
	}

	TArray<void*> RawData;
	Target.PropertyHandle->AccessRawData(RawData);
	if (RawData.IsEmpty())
	{
		return false;
	}

	Target.PropertyHandle->NotifyPreChange();

	const int32 EditableValueCount = FMath::Min(RawData.Num(), Baseline.BaselineValues.Num());
	for (int32 Index = 0; Index < EditableValueCount; ++Index)
	{
		void* RawPtr = RawData[Index];
		if (RawPtr == nullptr)
		{
			continue;
		}

		double NewValue = Baseline.BaselineValues[Index] + Delta;
		NewValue = Baseline.Constraints.Clamp(NewValue);

		if (Target.NumericType == EValueLadderNumericType::Int32)
		{
			NewValue = ValueLadder::Math::ApplyIntegerRounding(NewValue);
		}

		WriteScalar(RawPtr, Target.NumericType, NewValue);
	}

	Target.PropertyHandle->NotifyPostChange(bInteractive ? EPropertyChangeType::Interactive : EPropertyChangeType::ValueSet);
	if (!bInteractive)
	{
		Target.PropertyHandle->NotifyFinishedChangingProperties();
	}

	return true;
}

bool FPropertyHandleValueAdapter::RestoreBaseline(const FValueLadderPropertyTarget& Target, const FValueLadderBaselineData& Baseline) const
{
	return ApplyDelta(Target, Baseline, 0.0, false);
}

FString FPropertyHandleValueAdapter::BuildPreviewText(
	const FValueLadderPropertyTarget& Target,
	const FValueLadderBaselineData& Baseline,
	const double Delta) const
{
	if (Baseline.BaselineValues.IsEmpty())
	{
		return TEXT("-");
	}

	double PreviewValue = Baseline.Constraints.Clamp(Baseline.BaselineValues[0] + Delta);
	if (Target.NumericType == EValueLadderNumericType::Int32)
	{
		return FString::FromInt(FMath::RoundToInt(PreviewValue));
	}

	return FString::SanitizeFloat(PreviewValue);
}

bool FPropertyHandleValueAdapter::PopulateConstraints(
	const TSharedPtr<IPropertyHandle>& Handle,
	FValueLadderConstraintRange& OutConstraints) const
{
	double ParsedValue = 0.0;
	if (TryParseBound(Handle, NAME_ClampMin, ParsedValue) || TryParseBound(Handle, NAME_UIMin, ParsedValue))
	{
		OutConstraints.MinValue = ParsedValue;
	}

	if (TryParseBound(Handle, NAME_ClampMax, ParsedValue) || TryParseBound(Handle, NAME_UIMax, ParsedValue))
	{
		OutConstraints.MaxValue = ParsedValue;
	}

	return true;
}

bool FPropertyHandleValueAdapter::TryParseBound(
	const TSharedPtr<IPropertyHandle>& Handle,
	const FName& MetadataName,
	double& OutValue) const
{
	if (!Handle.IsValid())
	{
		return false;
	}

	const FString RawMetadata = Handle->GetMetaData(MetadataName);
	if (RawMetadata.IsEmpty())
	{
		return false;
	}

	OutValue = FCString::Atod(*RawMetadata);
	return true;
}

double FPropertyHandleValueAdapter::ReadScalar(void* RawData, const EValueLadderNumericType NumericType)
{
	switch (NumericType)
	{
	case EValueLadderNumericType::Float:
		return static_cast<double>(*static_cast<float*>(RawData));
	case EValueLadderNumericType::Double:
		return *static_cast<double*>(RawData);
	case EValueLadderNumericType::Int32:
		return static_cast<double>(*static_cast<int32*>(RawData));
	default:
		return 0.0;
	}
}

void FPropertyHandleValueAdapter::WriteScalar(void* RawData, const EValueLadderNumericType NumericType, const double NewValue)
{
	switch (NumericType)
	{
	case EValueLadderNumericType::Float:
		*static_cast<float*>(RawData) = static_cast<float>(NewValue);
		break;
	case EValueLadderNumericType::Double:
		*static_cast<double*>(RawData) = NewValue;
		break;
	case EValueLadderNumericType::Int32:
		*static_cast<int32*>(RawData) = FMath::RoundToInt(NewValue);
		break;
	default:
		break;
	}
}
