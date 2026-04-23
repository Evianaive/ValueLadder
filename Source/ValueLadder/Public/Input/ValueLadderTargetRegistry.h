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
		TWeakPtr<SWidget> Widget;
		FValueLadderPropertyTarget Target;
	};

	void Compact_NoLock();

	FCriticalSection RegistryMutex;
	TMap<FValueLadderTargetHandle, FRegisteredTarget> RegisteredTargets;
	FValueLadderTargetHandle NextHandle = 1;
};
