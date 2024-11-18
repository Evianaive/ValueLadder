#pragma once

#include "Framework/Application/IInputProcessor.h"


UENUM()
enum class EMouseEnterState : uint8
{
	Left,
	Middle,
	Right,
	None
};

class FValueLadderInputPreProcessor : public IInputProcessor
{
public:
	FValueLadderInputPreProcessor();
	
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent,	const FPointerEvent* InGestureEvent) override;
	
	EMouseEnterState MouseEnterState = EMouseEnterState::None;
	
	TWeakPtr<SWindow> ValueLadderWindow;
	int32 LockedIndex{-1};
	TVariant<int,float> LockedValue;
};
