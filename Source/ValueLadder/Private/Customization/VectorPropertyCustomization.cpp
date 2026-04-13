#include "Customization/VectorPropertyCustomization.h"

#include "ValueLadderLog.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "UObject/UnrealType.h"

namespace
{
	const TCHAR* ToNumericTypeString(const EValueLadderNumericType NumericType)
	{
		switch (NumericType)
		{
		case EValueLadderNumericType::Float:
			return TEXT("Float");
		case EValueLadderNumericType::Double:
			return TEXT("Double");
		case EValueLadderNumericType::Int32:
			return TEXT("Int32");
		default:
			return TEXT("Unknown");
		}
	}
}

FVectorPropertyCustomization::~FVectorPropertyCustomization()
{
	for (const FValueLadderTargetHandle Handle : RegisteredHandles)
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Customization][Vector] Unregister handle=%llu"), static_cast<uint64>(Handle));
		FValueLadderTargetRegistry::Get().UnregisterTarget(Handle);
	}
	RegisteredHandles.Reset();
}

TSharedRef<IPropertyTypeCustomization> FVectorPropertyCustomization::MakeInstance()
{
	return MakeShared<FVectorPropertyCustomization>();
}

void FVectorPropertyCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(140.0f)
	.MaxDesiredWidth(600.0f)
	[
		PropertyHandle->CreatePropertyValueWidget()
	];
}

void FVectorPropertyCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	uint32 NumChildren = 0;
	PropertyHandle->GetNumChildren(NumChildren);

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		const TSharedPtr<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(ChildIndex);
		if (!ChildHandle.IsValid() || !ChildHandle->IsValidHandle())
		{
			UE_LOG(LogValueLadder, Verbose, TEXT("[Customization][Vector] Skip child index=%u because handle is invalid."), ChildIndex);
			continue;
		}

		EValueLadderNumericType NumericType = EValueLadderNumericType::Float;
		if (!ResolveNumericType(ChildHandle.ToSharedRef(), NumericType))
		{
			UE_LOG(LogValueLadder, Verbose, TEXT("[Customization][Vector] Child '%s' is unsupported for ValueLadder registration; falling back to default row widgets."), *ChildHandle->GetPropertyDisplayName().ToString());

			TSharedRef<SWidget> NameWidget = ChildHandle->CreatePropertyNameWidget();
			TSharedRef<SWidget> ValueWidget = ChildHandle->CreatePropertyValueWidget();
			FDetailWidgetRow& FallbackRow = ChildBuilder.AddCustomRow(ChildHandle->GetPropertyDisplayName());
			FallbackRow
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
			continue;
		}

		TSharedRef<SWidget> NameWidget = ChildHandle->CreatePropertyNameWidget();
		TSharedRef<SWidget> ValueWidget = ChildHandle->CreatePropertyValueWidget();
		FValueLadderPropertyTarget Target;
		Target.PropertyHandle = ChildHandle;
		Target.NumericType = NumericType;
		Target.bIsVectorComponent = true;
		const FValueLadderTargetHandle NameHandle = FValueLadderTargetRegistry::Get().RegisterTarget(NameWidget, Target);
		const FValueLadderTargetHandle ValueHandle = FValueLadderTargetRegistry::Get().RegisterTarget(ValueWidget, Target);
		RegisteredHandles.Add(NameHandle);
		RegisteredHandles.Add(ValueHandle);
		UE_LOG(LogValueLadder, Display, TEXT("[Customization][Vector] Registered child '%s' nameHandle=%llu valueHandle=%llu nameWidget=%p valueWidget=%p type=%s"), *ChildHandle->GetPropertyDisplayName().ToString(), static_cast<uint64>(NameHandle), static_cast<uint64>(ValueHandle), static_cast<const void*>(&NameWidget.Get()), static_cast<const void*>(&ValueWidget.Get()), ToNumericTypeString(NumericType));

		FDetailWidgetRow& Row = ChildBuilder.AddCustomRow(ChildHandle->GetPropertyDisplayName());
		Row
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
	}
}

bool FVectorPropertyCustomization::ResolveNumericType(const TSharedRef<IPropertyHandle>& PropertyHandle, EValueLadderNumericType& OutType)
{
	const FProperty* Property = PropertyHandle->GetProperty();
	if (Property == nullptr)
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

	return false;
}
