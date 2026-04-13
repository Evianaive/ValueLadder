#include "Customization/NumericPropertyCustomization.h"

#include "ValueLadderLog.h"
#include "DetailWidgetRow.h"
#include "Layout/Children.h"
#include "PropertyHandle.h"
#include "UObject/UnrealType.h"
#include "Widgets/SWidget.h"

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

	void RegisterWidgetSubtree(
		const TSharedRef<SWidget>& RootWidget,
		const FValueLadderPropertyTarget& Target,
		TArray<FValueLadderTargetHandle>& OutHandles)
	{
		OutHandles.Add(FValueLadderTargetRegistry::Get().RegisterTarget(RootWidget, Target));

		FChildren* Children = RootWidget->GetAllChildren();
		if (Children == nullptr)
		{
			return;
		}

		for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
		{
			RegisterWidgetSubtree(Children->GetChildAt(ChildIndex), Target, OutHandles);
		}
	}
}

FNumericPropertyCustomization::~FNumericPropertyCustomization()
{
	for (const FValueLadderTargetHandle RegisteredHandle : RegisteredHandles)
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Customization][Numeric] Unregister handle=%llu"), static_cast<uint64>(RegisteredHandle));
		FValueLadderTargetRegistry::Get().UnregisterTarget(RegisteredHandle);
	}
	RegisteredHandles.Reset();
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

	TSharedRef<SWidget> NameWidget = PropertyHandle->CreatePropertyNameWidget();
	TSharedRef<SWidget> ValueWidget = PropertyHandle->CreatePropertyValueWidget();
	FValueLadderPropertyTarget Target;
	Target.PropertyHandle = PropertyHandle;
	Target.NumericType = NumericType;
	for (const FValueLadderTargetHandle RegisteredHandle : RegisteredHandles)
	{
		FValueLadderTargetRegistry::Get().UnregisterTarget(RegisteredHandle);
	}
	RegisteredHandles.Reset();
	RegisterWidgetSubtree(NameWidget, Target, RegisteredHandles);
	RegisterWidgetSubtree(ValueWidget, Target, RegisteredHandles);
	
	// Additional validation for debugging
	const bool bHandleValid = PropertyHandle->IsValidHandle();
	UE_LOG(LogValueLadder, Display, TEXT("[Customization][Numeric] Registered property '%s' subtreeHandles=%d nameWidget=%p valueWidget=%p type=%s IsValidHandle=%s"), 
		*PropertyHandle->GetPropertyDisplayName().ToString(), 
		RegisteredHandles.Num(), 
		static_cast<const void*>(&NameWidget.Get()), 
		static_cast<const void*>(&ValueWidget.Get()), 
		ToNumericTypeString(NumericType),
		bHandleValid ? TEXT("true") : TEXT("false"));

	HeaderRow
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

void FNumericPropertyCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
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

	if (Property->IsA<FIntProperty>())
	{
		OutType = EValueLadderNumericType::Int32;
		return true;
	}

	return false;
}
