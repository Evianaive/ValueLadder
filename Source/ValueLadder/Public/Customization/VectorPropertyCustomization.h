#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

#include "Input/ValueLadderTargetRegistry.h"

class FVectorPropertyCustomization : public IPropertyTypeCustomization
{
public:
	virtual ~FVectorPropertyCustomization() override;

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

	TArray<FValueLadderTargetHandle> RegisteredHandles;
};
