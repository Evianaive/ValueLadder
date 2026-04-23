#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

#include "Input/ValueLadderTargetRegistry.h"

class FIntPointPropertyCustomization : public IPropertyTypeCustomization
{
public:
	virtual ~FIntPointPropertyCustomization() override;

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
	static TSharedPtr<IPropertyHandle> FindChildHandleByPropertyName(const TSharedRef<IPropertyHandle>& PropertyHandle, const FName& PropertyName);
	static bool ResolveNumericType(const TSharedRef<IPropertyHandle>& PropertyHandle, EValueLadderNumericType& OutType);

	void RegisterWidgetSubtree(const TSharedRef<SWidget>& RootWidget, const FValueLadderPropertyTarget& Target);

	TArray<FValueLadderTargetHandle> RegisteredHandles;
};
