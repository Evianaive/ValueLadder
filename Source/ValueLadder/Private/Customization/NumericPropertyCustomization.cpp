#include "Customization/NumericPropertyCustomization.h"

#include "ValueLadderLog.h"
#include "DetailWidgetRow.h"
#include "HAL/PlatformTime.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/Children.h"
#include "PropertyHandle.h"
#include "Types/ISlateMetaData.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWidget.h"

namespace
{
	constexpr double RegisterSubtreePerfLogThresholdUs = 500.0;

	const TCHAR* ToNumericTypeString(const EValueLadderNumericType NumericType)
	{
		return ValueLadder::ToNumericTypeString(NumericType);
	}

	FName MakeHandleTag(const FValueLadderTargetHandle Handle)
	{
		return FName(*FString::Printf(TEXT("ValueLadder.Handle.%llu"), static_cast<uint64>(Handle)));
	}

	FName MakeDetailRowTag(const FString& PropertyDisplayName)
	{
		return FName(*FString::Printf(TEXT("DetailRowItem.%s"), *PropertyDisplayName));
	}

	void TagWidgetTree(const TSharedRef<SWidget>& RootWidget, const FName Tag)
	{
		TArray<TSharedRef<SWidget>> PendingWidgets;
		PendingWidgets.Add(RootWidget);
		while (PendingWidgets.Num() > 0)
		{
			const TSharedRef<SWidget> Widget = PendingWidgets.Pop(EAllowShrinking::No);
			Widget->AddMetadata(MakeShared<FTagMetaData>(Tag));

			FChildren* Children = Widget->GetAllChildren();
			if (Children == nullptr)
			{
				continue;
			}

			for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
			{
				PendingWidgets.Add(Children->GetChildAt(ChildIndex));
			}
		}
	}
}

FNumericPropertyCustomization::~FNumericPropertyCustomization()
{
	ClearRegisteredHandles();
}

TSharedRef<IPropertyTypeCustomization> FNumericPropertyCustomization::MakeInstance()
{
	return MakeShared<FNumericPropertyCustomization>();
}

void FNumericPropertyCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	EValueLadderNumericType NumericType = EValueLadderNumericType::Float;
	if (!ResolveNumericType(PropertyHandle, NumericType))
	{
		UE_LOG(LogValueLadder, Verbose, TEXT("[Customization][Numeric] Property '%s' has unsupported numeric type; using default widget."), *PropertyHandle->GetPropertyDisplayName().ToString());
		HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			PropertyHandle->CreatePropertyValueWidget()
		];
		return;
	}

	const TSharedRef<SWidget> RawNameWidget = PropertyHandle->CreatePropertyNameWidget();
	const TSharedRef<SWidget> RawValueWidget = PropertyHandle->CreatePropertyValueWidget();
	const TSharedRef<SBox> NameWidget = SNew(SBox)
	[
		RawNameWidget
	];
	const TSharedRef<SBox> ValueWidget = SNew(SBox)
	[
		RawValueWidget
	];
	FValueLadderPropertyTarget Target;
	Target.PropertyHandle = PropertyHandle;
	Target.NumericType = NumericType;
	Target.SemanticRole = GetDefaultSemanticRole(NumericType);
	const FString PropertyDisplayName = PropertyHandle->GetPropertyDisplayName().ToString();

	HeaderRow
	.RowTag(MakeDetailRowTag(PropertyDisplayName))
	.NameContent()
	[
		NameWidget
	]
	.ValueContent()
	.MinDesiredWidth(140.0f)
	.MaxDesiredWidth(600.0f)
	[
		ValueWidget
	];

	RegisterLiveWidgetSubtrees(NameWidget, ValueWidget, Target, PropertyDisplayName);
}

void FNumericPropertyCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FNumericPropertyCustomization::ClearRegisteredHandles()
{
	for (const FValueLadderTargetHandle RegisteredHandle : RegisteredHandles)
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Customization][Numeric] Unregister handle=%llu"), static_cast<uint64>(RegisteredHandle));
		FValueLadderTargetRegistry::Get().UnregisterTarget(RegisteredHandle);
	}

	RegisteredHandles.Reset();
}

void FNumericPropertyCustomization::RegisterLiveWidgetSubtrees(
	const TSharedRef<SWidget>& NameWidget,
	const TSharedRef<SWidget>& ValueWidget,
	const FValueLadderPropertyTarget& Target,
	const FString& PropertyDisplayName)
{
	const double StartTimeSeconds = FPlatformTime::Seconds();
	ClearRegisteredHandles();

	const FValueLadderTargetHandle NameRootHandle = FValueLadderTargetRegistry::Get().RegisterTarget(NameWidget, Target);
	const FValueLadderTargetHandle ValueRootHandle = FValueLadderTargetRegistry::Get().RegisterTarget(ValueWidget, Target);
	RegisteredHandles.Add(NameRootHandle);
	RegisteredHandles.Add(ValueRootHandle);

	if (NameRootHandle != 0)
	{
		TagWidgetTree(NameWidget, MakeHandleTag(NameRootHandle));
	}

	if (ValueRootHandle != 0)
	{
		TagWidgetTree(ValueWidget, MakeHandleTag(ValueRootHandle));
	}

	const bool bHandleValid = Target.PropertyHandle.IsValid() && Target.PropertyHandle->IsValidHandle();
	UE_LOG(
		LogValueLadder,
		Display,
		TEXT("[Customization][Numeric] Registered property '%s' subtreeHandles=%d nameWidget=%p valueWidget=%p type=%s IsValidHandle=%s"),
		*PropertyDisplayName,
		RegisteredHandles.Num(),
		static_cast<const void*>(&NameWidget.Get()),
		static_cast<const void*>(&ValueWidget.Get()),
		ToNumericTypeString(Target.NumericType),
		bHandleValid ? TEXT("true") : TEXT("false"));

	const double DurationUs = (FPlatformTime::Seconds() - StartTimeSeconds) * 1000000.0;
	if (DurationUs >= RegisterSubtreePerfLogThresholdUs)
	{
		UE_LOG(
			LogValueLadder,
			Display,
			TEXT("[Perf][Customization][Numeric] RegisterLiveWidgetSubtrees property='%s' handles=%d duration=%.1fus"),
			*PropertyDisplayName,
			RegisteredHandles.Num(),
			DurationUs);
	}
	else
	{
		UE_LOG(
			LogValueLadder,
			VeryVerbose,
			TEXT("[Perf][Customization][Numeric] RegisterLiveWidgetSubtrees property='%s' handles=%d duration=%.1fus"),
			*PropertyDisplayName,
			RegisteredHandles.Num(),
			DurationUs);
	}
}

bool FNumericPropertyCustomization::ResolveNumericType(
	const TSharedRef<IPropertyHandle>& PropertyHandle,
	EValueLadderNumericType& OutType)
{
	const FProperty* Property = PropertyHandle->GetProperty();
	if (Property == nullptr)
	{
		return false;
	}

	if (Property->HasMetaData(TEXT("Bitmask")) || Property->IsA<FEnumProperty>())
	{
		return false;
	}

	if (Property->IsA<FFloatProperty>())
	{
		OutType = EValueLadderNumericType::Float;
		return true;
	}

	if (Property->IsA<FDoubleProperty>())
	{
		OutType = EValueLadderNumericType::Double;
		return true;
	}

	if (Property->IsA<FByteProperty>())
	{
		const FByteProperty* ByteProperty = CastField<const FByteProperty>(Property);
		if (ByteProperty == nullptr || ByteProperty->Enum != nullptr)
		{
			return false;
		}

		OutType = EValueLadderNumericType::UInt8;
		return true;
	}

	if (Property->IsA<FInt8Property>())
	{
		OutType = EValueLadderNumericType::Int8;
		return true;
	}

	if (Property->IsA<FInt16Property>())
	{
		OutType = EValueLadderNumericType::Int16;
		return true;
	}

	if (Property->IsA<FIntProperty>())
	{
		OutType = EValueLadderNumericType::Int32;
		return true;
	}

	if (Property->IsA<FInt64Property>())
	{
		OutType = EValueLadderNumericType::Int64;
		return true;
	}

	if (Property->IsA<FUInt16Property>())
	{
		OutType = EValueLadderNumericType::UInt16;
		return true;
	}

	if (Property->IsA<FUInt32Property>())
	{
		OutType = EValueLadderNumericType::UInt32;
		return true;
	}

	if (Property->IsA<FUInt64Property>())
	{
		OutType = EValueLadderNumericType::UInt64;
		return true;
	}

	return false;
}
