#pragma once

#include "CoreMinimal.h"

#include "Adapter/PropertyHandleValueAdapter.h"

class SWidget;
class FWidgetPath;

using FValueLadderTargetHandle = uint64;

class FValueLadderTargetRegistry
{
public:
	static FValueLadderTargetRegistry& Get();

	FValueLadderTargetHandle RegisterTarget(const TSharedRef<SWidget>& Widget, const FValueLadderPropertyTarget& Target);
	void UnregisterTarget(FValueLadderTargetHandle Handle);
	bool ResolveTargetFromWidgetPath(const FWidgetPath& WidgetPath, FValueLadderPropertyTarget& OutTarget);
	bool FindRegisteredWidgetForPropertyName(const FName PropertyName, TSharedPtr<SWidget>& OutWidget, FValueLadderPropertyTarget& OutTarget);

private:
	struct FRegisteredTarget
	{
		const SWidget* WidgetKey = nullptr;
		TWeakPtr<SWidget> Widget;
		FValueLadderPropertyTarget Target;
		FString NormalizedPropertyToken;
		FString NormalizedDisplayToken;
	};

	void MaybeCompact_NoLock(bool bForce = false);
	void Compact_NoLock();

	FCriticalSection RegistryMutex;
	TMap<FValueLadderTargetHandle, FRegisteredTarget> RegisteredTargets;
	TMap<const SWidget*, TArray<FValueLadderTargetHandle>> WidgetIndex;
	TMap<FString, TArray<FValueLadderTargetHandle>> PropertyTokenIndex;
	TMap<FString, TArray<FValueLadderTargetHandle>> DisplayTokenIndex;
	int32 MutationsSinceCompact = 0;
	int32 StaleHintCount = 0;
	bool bCompactRequested = false;
	FValueLadderTargetHandle NextHandle = 1;
};
