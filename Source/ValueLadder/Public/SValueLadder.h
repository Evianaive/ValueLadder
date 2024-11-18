// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class VALUELADDER_API SValueLadder : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SValueLadder)
		:_LadderValues()
		,_Index(-1)
		{}
		SLATE_ARGUMENT(TArray<FText>, LadderValues)
		SLATE_ATTRIBUTE(int32, Index)
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
	void SetIndex( TAttribute<int32> InValue )
	{
		Index = MoveTemp(InValue);
	}
	float GetIndex() const
	{
		return Index.Get();
	}
	TAttribute<int32> Index;
};
