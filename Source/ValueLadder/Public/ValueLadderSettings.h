// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ValueLadderSettings.generated.h"

/**
 * 
 */
UCLASS(Config=ValueLadder, DefaultConfig)
class VALUELADDER_API UValueLadderSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	virtual FName GetContainerName() const { return FName("Editor"); }
	virtual FName GetCategoryName() const { return FName("Plugins"); }
	virtual FName GetSectionName() const { return FName("Value Ladder"); }

	UPROPERTY(EditAnywhere)
	TArray<float> FloatLadders {0.1,1,10,100,1000,10000};
	UPROPERTY(EditAnywhere)
	int32 DefaultFloatLadderIndex = 3;
	UPROPERTY(EditAnywhere)
	TArray<int32> IntLadders {1,10,100,1000};
	UPROPERTY(EditAnywhere)
	int32 DefaultIntLadderIndex = 0;

	template<typename T>
	const TArray<T>& GetLadderValues() const;
	template<typename T>
	int32 GetDefaultLadderValuesIndex() const;
};
