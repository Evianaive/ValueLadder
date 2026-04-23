#include "Adapter/PropertyHandleValueAdapter.h"

#include "ValueLadderLog.h"

#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "UObject/UnrealType.h"

#include <limits>
#include <type_traits>

namespace
{
const FName VLT_NAME_ClampMin(TEXT("ClampMin"));
const FName VLT_NAME_ClampMax(TEXT("ClampMax"));
const FName VLT_NAME_UIMin(TEXT("UIMin"));
const FName VLT_NAME_UIMax(TEXT("UIMax"));
const FName VLT_NAME_X(TEXT("X"));
const FName VLT_NAME_Y(TEXT("Y"));
const FName VLT_NAME_Z(TEXT("Z"));
const FName VLT_NAME_Roll(TEXT("Roll"));
const FName VLT_NAME_Pitch(TEXT("Pitch"));
const FName VLT_NAME_Yaw(TEXT("Yaw"));

	const TCHAR* ToNumericTypeString(const EValueLadderNumericType NumericType)
	{
		return ValueLadder::ToNumericTypeString(NumericType);
	}

	const TCHAR* ToTargetKindString(const FValueLadderPropertyTarget::ETargetKind Kind)
	{
		switch (Kind)
		{
		case FValueLadderPropertyTarget::ETargetKind::PropertyHandleScalar:
			return TEXT("PropertyHandleScalar");
		case FValueLadderPropertyTarget::ETargetKind::TransformProxy:
			return TEXT("TransformProxy");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* ToTransformFieldString(const FValueLadderPropertyTarget::ETransformField Field)
	{
		switch (Field)
		{
		case FValueLadderPropertyTarget::ETransformField::Location:
			return TEXT("Location");
		case FValueLadderPropertyTarget::ETransformField::Rotation:
			return TEXT("Rotation");
		case FValueLadderPropertyTarget::ETransformField::Scale:
			return TEXT("Scale");
		default:
			return TEXT("Unknown");
		}
	}

	template <typename NumericT>
	double ReadTransformComponent(const UE::Math::TTransform<NumericT>& Transform, const FValueLadderPropertyTarget& Target)
	{
		if (Target.TransformField == FValueLadderPropertyTarget::ETransformField::Rotation)
		{
			const UE::Math::TRotator<NumericT> Rotation = Transform.GetRotation().Rotator();
			if (Target.ComponentName == VLT_NAME_Roll)
			{
				return static_cast<double>(Rotation.Roll);
			}

			if (Target.ComponentName == VLT_NAME_Pitch)
			{
				return static_cast<double>(Rotation.Pitch);
			}

			return static_cast<double>(Rotation.Yaw);
		}

		const UE::Math::TVector<NumericT> Value = Target.TransformField == FValueLadderPropertyTarget::ETransformField::Scale
			? Transform.GetScale3D()
			: Transform.GetTranslation();

		if (Target.ComponentName == VLT_NAME_Y)
		{
			return static_cast<double>(Value.Y);
		}

		if (Target.ComponentName == VLT_NAME_Z)
		{
			return static_cast<double>(Value.Z);
		}

		return static_cast<double>(Value.X);
	}

	template <typename NumericT>
	void WriteTransformComponent(UE::Math::TTransform<NumericT>& Transform, const FValueLadderPropertyTarget& Target, const double NewValue)
	{
		const NumericT ConvertedValue = static_cast<NumericT>(NewValue);

		if (Target.TransformField == FValueLadderPropertyTarget::ETransformField::Rotation)
		{
			UE::Math::TRotator<NumericT> Rotation = Transform.GetRotation().Rotator();
			if (Target.ComponentName == VLT_NAME_Roll)
			{
				Rotation.Roll = ConvertedValue;
			}
			else if (Target.ComponentName == VLT_NAME_Pitch)
			{
				Rotation.Pitch = ConvertedValue;
			}
			else
			{
				Rotation.Yaw = ConvertedValue;
			}

			Transform.SetRotation(Rotation.Quaternion());
			return;
		}

		UE::Math::TVector<NumericT> Value = Target.TransformField == FValueLadderPropertyTarget::ETransformField::Scale
			? Transform.GetScale3D()
			: Transform.GetTranslation();

		if (Target.ComponentName == VLT_NAME_Y)
		{
			Value.Y = ConvertedValue;
		}
		else if (Target.ComponentName == VLT_NAME_Z)
		{
			Value.Z = ConvertedValue;
		}
		else
		{
			Value.X = ConvertedValue;
		}

		if (Target.TransformField == FValueLadderPropertyTarget::ETransformField::Scale)
		{
			Transform.SetScale3D(Value);
		}
		else
		{
			Transform.SetTranslation(Value);
		}
	}

	template <typename NumericT>
	double ReadScalarAsDouble(void* RawData)
	{
		return static_cast<double>(*static_cast<NumericT*>(RawData));
	}

	template <typename NumericT>
	NumericT ClampRoundedInteger(const double NewValue)
	{
		const double RoundedValue = ValueLadder::Math::ApplyIntegerRounding(NewValue);
		const double MinValue = static_cast<double>(std::numeric_limits<NumericT>::lowest());
		const double MaxValue = static_cast<double>(std::numeric_limits<NumericT>::max());
		return static_cast<NumericT>(FMath::Clamp(RoundedValue, MinValue, MaxValue));
	}

	template <typename NumericT>
	FString BuildIntegerPreviewString(const double PreviewValue)
	{
		return LexToString(ClampRoundedInteger<NumericT>(PreviewValue));
	}
}

bool FPropertyHandleValueAdapter::CaptureBaseline(
	const FValueLadderPropertyTarget& Target,
	FValueLadderBaselineData& OutBaseline,
	FString& OutError) const
{
	if (!Target.IsValid())
	{
		OutError = TEXT("Property target is invalid.");
		UE_LOG(LogValueLadder, Warning, TEXT("[Adapter] CaptureBaseline failed: %s"), *OutError);
		return false;
	}

	if (!Target.PropertyHandle->IsEditable() || Target.PropertyHandle->IsEditConst())
	{
		OutError = TEXT("Property is not editable.");
		UE_LOG(LogValueLadder, Warning, TEXT("[Adapter] CaptureBaseline failed for kind=%s type=%s: %s"), ToTargetKindString(Target.Kind), ToNumericTypeString(Target.NumericType), *OutError);
		return false;
	}

	TArray<void*> RawData;
	Target.PropertyHandle->AccessRawData(RawData);
	if (RawData.IsEmpty())
	{
		OutError = TEXT("No raw property data found for current selection.");
		UE_LOG(LogValueLadder, Warning, TEXT("[Adapter] CaptureBaseline failed for kind=%s type=%s: %s"), ToTargetKindString(Target.Kind), ToNumericTypeString(Target.NumericType), *OutError);
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

		if (Target.Kind == FValueLadderPropertyTarget::ETargetKind::TransformProxy)
		{
			switch (Target.NumericType)
			{
			case EValueLadderNumericType::Float:
				OutBaseline.BaselineValues.Add(ReadTransformComponent(*static_cast<FTransform3f*>(RawPtr), Target));
				break;
			case EValueLadderNumericType::Double:
				OutBaseline.BaselineValues.Add(ReadTransformComponent(*static_cast<FTransform3d*>(RawPtr), Target));
				break;
			default:
				OutError = TEXT("Transform proxy targets only support float and double components.");
				UE_LOG(LogValueLadder, Warning, TEXT("[Adapter] CaptureBaseline failed for kind=%s field=%s component=%s: %s"), ToTargetKindString(Target.Kind), ToTransformFieldString(Target.TransformField), *Target.ComponentName.ToString(), *OutError);
				return false;
			}
		}
		else
		{
			OutBaseline.BaselineValues.Add(ReadScalar(RawPtr, Target.NumericType));
		}
	}

	if (OutBaseline.BaselineValues.IsEmpty())
	{
		OutError = TEXT("Failed to capture baseline values.");
		UE_LOG(LogValueLadder, Warning, TEXT("[Adapter] CaptureBaseline failed for kind=%s type=%s: %s"), ToTargetKindString(Target.Kind), ToNumericTypeString(Target.NumericType), *OutError);
		return false;
	}

	if (Target.Kind == FValueLadderPropertyTarget::ETargetKind::PropertyHandleScalar)
	{
		PopulateConstraints(Target.PropertyHandle, OutBaseline.Constraints);
	}

	UE_LOG(LogValueLadder, Display, TEXT("[Adapter] CaptureBaseline success kind=%s type=%s values=%d"), ToTargetKindString(Target.Kind), ToNumericTypeString(Target.NumericType), OutBaseline.BaselineValues.Num());
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
		UE_LOG(LogValueLadder, Warning, TEXT("[Adapter] ApplyDelta failed: invalid target or empty baseline."));
		return false;
	}

	TArray<void*> RawData;
	Target.PropertyHandle->AccessRawData(RawData);
	if (RawData.IsEmpty())
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Adapter] ApplyDelta failed: no raw data available."));
		return false;
	}

	UE_LOG(LogValueLadder, VeryVerbose, TEXT("[Adapter] ApplyDelta kind=%s type=%s delta=%.6g interactive=%s values=%d"), ToTargetKindString(Target.Kind), ToNumericTypeString(Target.NumericType), Delta, bInteractive ? TEXT("true") : TEXT("false"), Baseline.BaselineValues.Num());

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

		if (ValueLadder::IsIntegerNumericType(Target.NumericType))
		{
			NewValue = ValueLadder::Math::ApplyIntegerRounding(NewValue);
		}

		if (Target.Kind == FValueLadderPropertyTarget::ETargetKind::TransformProxy)
		{
			switch (Target.NumericType)
			{
			case EValueLadderNumericType::Float:
				WriteTransformComponent(*static_cast<FTransform3f*>(RawPtr), Target, NewValue);
				break;
			case EValueLadderNumericType::Double:
				WriteTransformComponent(*static_cast<FTransform3d*>(RawPtr), Target, NewValue);
				break;
			default:
				UE_LOG(LogValueLadder, Warning, TEXT("[Adapter] ApplyDelta failed: unsupported transform proxy type=%s."), ToNumericTypeString(Target.NumericType));
				return false;
			}
		}
		else
		{
			WriteScalar(RawPtr, Target.NumericType, NewValue);
		}
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
	UE_LOG(LogValueLadder, Display, TEXT("[Adapter] RestoreBaseline requested."));
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
	if (ValueLadder::IsIntegerNumericType(Target.NumericType))
	{
		switch (Target.NumericType)
		{
		case EValueLadderNumericType::Int8:
			return BuildIntegerPreviewString<int8>(PreviewValue);
		case EValueLadderNumericType::UInt8:
			return BuildIntegerPreviewString<uint8>(PreviewValue);
		case EValueLadderNumericType::Int16:
			return BuildIntegerPreviewString<int16>(PreviewValue);
		case EValueLadderNumericType::UInt16:
			return BuildIntegerPreviewString<uint16>(PreviewValue);
		case EValueLadderNumericType::Int32:
			return BuildIntegerPreviewString<int32>(PreviewValue);
		case EValueLadderNumericType::UInt32:
			return BuildIntegerPreviewString<uint32>(PreviewValue);
		case EValueLadderNumericType::Int64:
			return BuildIntegerPreviewString<int64>(PreviewValue);
		case EValueLadderNumericType::UInt64:
			return BuildIntegerPreviewString<uint64>(PreviewValue);
		default:
			break;
		}
	}

	return FString::SanitizeFloat(PreviewValue);
}

bool FPropertyHandleValueAdapter::PopulateConstraints(
	const TSharedPtr<IPropertyHandle>& Handle,
	FValueLadderConstraintRange& OutConstraints) const
{
	double ParsedValue = 0.0;
	if (TryParseBound(Handle, VLT_NAME_ClampMin, ParsedValue) || TryParseBound(Handle, VLT_NAME_UIMin, ParsedValue))
	{
		OutConstraints.MinValue = ParsedValue;
	}

	if (TryParseBound(Handle, VLT_NAME_ClampMax, ParsedValue) || TryParseBound(Handle, VLT_NAME_UIMax, ParsedValue))
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
		return ReadScalarAsDouble<float>(RawData);
	case EValueLadderNumericType::Double:
		return ReadScalarAsDouble<double>(RawData);
	case EValueLadderNumericType::Int8:
		return ReadScalarAsDouble<int8>(RawData);
	case EValueLadderNumericType::UInt8:
		return ReadScalarAsDouble<uint8>(RawData);
	case EValueLadderNumericType::Int16:
		return ReadScalarAsDouble<int16>(RawData);
	case EValueLadderNumericType::UInt16:
		return ReadScalarAsDouble<uint16>(RawData);
	case EValueLadderNumericType::Int32:
		return ReadScalarAsDouble<int32>(RawData);
	case EValueLadderNumericType::UInt32:
		return ReadScalarAsDouble<uint32>(RawData);
	case EValueLadderNumericType::Int64:
		return ReadScalarAsDouble<int64>(RawData);
	case EValueLadderNumericType::UInt64:
		return ReadScalarAsDouble<uint64>(RawData);
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
	case EValueLadderNumericType::Int8:
		*static_cast<int8*>(RawData) = ClampRoundedInteger<int8>(NewValue);
		break;
	case EValueLadderNumericType::UInt8:
		*static_cast<uint8*>(RawData) = ClampRoundedInteger<uint8>(NewValue);
		break;
	case EValueLadderNumericType::Int16:
		*static_cast<int16*>(RawData) = ClampRoundedInteger<int16>(NewValue);
		break;
	case EValueLadderNumericType::UInt16:
		*static_cast<uint16*>(RawData) = ClampRoundedInteger<uint16>(NewValue);
		break;
	case EValueLadderNumericType::Int32:
		*static_cast<int32*>(RawData) = ClampRoundedInteger<int32>(NewValue);
		break;
	case EValueLadderNumericType::UInt32:
		*static_cast<uint32*>(RawData) = ClampRoundedInteger<uint32>(NewValue);
		break;
	case EValueLadderNumericType::Int64:
		*static_cast<int64*>(RawData) = ClampRoundedInteger<int64>(NewValue);
		break;
	case EValueLadderNumericType::UInt64:
		*static_cast<uint64*>(RawData) = ClampRoundedInteger<uint64>(NewValue);
		break;
	default:
		break;
	}
}
