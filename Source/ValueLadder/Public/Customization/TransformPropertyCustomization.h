#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

#include "Input/ValueLadderTargetRegistry.h"

class FTransformPropertyCustomization : public IPropertyTypeCustomization
{
public:
	virtual ~FTransformPropertyCustomization() override;

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
	static TSharedPtr<IPropertyHandle> FindChildHandleByPropertyName(const TSharedRef<IPropertyHandle>& PropertyHandle, const FName& PropertyName);
	static void CollectWidgetsByTypePrefix(const TSharedRef<SWidget>& RootWidget, const FString& TypePrefix, TArray<TSharedRef<SWidget>>& OutWidgets);

	void RegisterWidgetSubtree(const TSharedRef<SWidget>& RootWidget, const FValueLadderPropertyTarget& Target);
	void RegisterNumericEntrySubtrees(const TSharedRef<SWidget>& RootWidget, const TArray<FValueLadderPropertyTarget>& Targets, const TCHAR* ContextLabel);
	void AddDirectVectorRow(
		IDetailChildrenBuilder& ChildBuilder,
		const TSharedRef<IPropertyHandle>& GroupHandle,
		const FText& DisplayName,
		FValueLadderPropertyTarget::ETransformField TransformField,
		EValueLadderNumericType NumericType,
		const TCHAR* ContextLabel);
	void AddRotationProxyRow(
		IDetailChildrenBuilder& ChildBuilder,
		const TSharedRef<IPropertyHandle>& TransformHandle,
		const TSharedRef<IPropertyHandle>& RotationHandle,
		EValueLadderNumericType NumericType);

	TArray<FValueLadderTargetHandle> RegisteredHandles;
};
