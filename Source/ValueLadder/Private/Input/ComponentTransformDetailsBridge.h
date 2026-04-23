#pragma once

#include "CoreMinimal.h"

#include "Adapter/PropertyHandleValueAdapter.h"

class FPropertyEditorModule;
class FWidgetPath;
class IDetailsView;
class IPropertyHandle;
class SWidget;
struct FOnGenerateGlobalRowExtensionArgs;
struct FPropertyRowExtensionButton;

class FComponentTransformDetailsBridge
{
public:
	static FComponentTransformDetailsBridge& Get();

	void Register(FPropertyEditorModule& PropertyModule);
	void Unregister(FPropertyEditorModule& PropertyModule);
	bool ResolveTargetFromWidgetPath(const FWidgetPath& WidgetPath, FValueLadderPropertyTarget& OutTarget);

	struct FCachedTransformField
	{
		TSharedPtr<IPropertyHandle> PropertyHandle;
		EValueLadderNumericType NumericType = EValueLadderNumericType::Float;
	};

	struct FCachedScalarRow
	{
		TSharedPtr<IPropertyHandle> PropertyHandle;
		EValueLadderNumericType NumericType = EValueLadderNumericType::Float;
		EValueLadderSemanticRole SemanticRole = EValueLadderSemanticRole::GenericScalar;
		FName PropertyName = NAME_None;
		FString PropertyDisplayName;
		FString PropertyPath;
	};

	struct FDetailsViewTransformCache
	{
		TWeakPtr<SWidget> DetailsViewWidget;
		FCachedTransformField Location;
		FCachedTransformField Rotation;
		FCachedTransformField Scale;
		TArray<FCachedScalarRow> ScalarRows;
	};

private:
	void HandleGenerateGlobalRowExtension(const FOnGenerateGlobalRowExtensionArgs& InArgs, TArray<FPropertyRowExtensionButton>& OutExtensions);
	void CacheTransformField(const TSharedRef<IDetailsView>& DetailsView, FValueLadderPropertyTarget::ETransformField Field, const TSharedRef<IPropertyHandle>& PropertyHandle);
	void CacheScalarNumericRow(const TSharedRef<IDetailsView>& DetailsView, const TSharedRef<IPropertyHandle>& PropertyHandle);
	void CompactStaleEntries();

	static bool ResolveNumericType(const TSharedRef<IPropertyHandle>& PropertyHandle, EValueLadderNumericType& OutType);
	static TSharedPtr<IPropertyHandle> FindChildHandleByPropertyName(const TSharedRef<IPropertyHandle>& PropertyHandle, const FName& PropertyName);
	static void CollectWidgetsByTypePrefix(const TSharedRef<SWidget>& RootWidget, const FString& TypePrefix, TArray<TSharedRef<SWidget>>& OutWidgets);
	static const SWidget* GetDetailsViewWidgetKey(const TSharedRef<IDetailsView>& DetailsView);
	static const SWidget* FindDetailsViewWidgetKey(const FWidgetPath& WidgetPath);
	static bool TryResolveFieldFromWidgetPath(const FWidgetPath& WidgetPath, FValueLadderPropertyTarget::ETransformField& OutField);
	static bool TryResolveComponentFromWidgetPath(
		const FWidgetPath& WidgetPath,
		FValueLadderPropertyTarget::ETransformField Field,
		FName& OutComponentName,
		FString& OutContainerType,
		int32& OutDisplayIndex,
		int32& OutComponentIndex);
	bool TryResolveScalarNumericFromWidgetPath(const FWidgetPath& WidgetPath, FValueLadderPropertyTarget& OutTarget);
	static bool TryExtractDetailRowToken(const FWidgetPath& WidgetPath, FString& OutRowToken);
	static FString NormalizeRowToken(const FString& InToken);

	TMap<const SWidget*, FDetailsViewTransformCache> CachedFieldsByDetailsView;
	FDelegateHandle RowExtensionDelegateHandle;
};
