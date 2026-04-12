#include "Customization/NumericPropertyCustomization.h"

#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "UObject/UnrealType.h"

FNumericPropertyCustomization::~FNumericPropertyCustomization()
{
	if (RegisteredHandle != 0)
	{
		FValueLadderTargetRegistry::Get().UnregisterTarget(RegisteredHandle);
		RegisteredHandle = 0;
	}
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

	TSharedRef<SWidget> ValueWidget = PropertyHandle->CreatePropertyValueWidget();
	FValueLadderPropertyTarget Target;
	Target.PropertyHandle = PropertyHandle;
	Target.NumericType = NumericType;
	RegisteredHandle = FValueLadderTargetRegistry::Get().RegisterTarget(ValueWidget, Target);

	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
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
