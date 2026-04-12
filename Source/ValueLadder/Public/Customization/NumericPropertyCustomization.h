#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

#include "Input/ValueLadderTargetRegistry.h"

class FNumericPropertyCustomization : public IPropertyTypeCustomization
{
public:
	virtual ~FNumericPropertyCustomization() override;

	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> PropertyHandle,
		FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> PropertyHandle,
		IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	static bool ResolveNumericType(const TSharedRef<IPropertyHandle>& PropertyHandle, EValueLadderNumericType& OutType);

	FValueLadderTargetHandle RegisteredHandle = 0;
};
