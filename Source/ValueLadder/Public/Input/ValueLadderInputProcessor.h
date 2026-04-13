#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"

#include "Session/ValueLadderSession.h"

class SValueLadderOverlay;
class SWindow;

class FValueLadderInputProcessor : public IInputProcessor
{
public:
	FValueLadderInputProcessor();

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;

private:
	bool InitializeGesture(const UValueLadderSettings& Settings, FSlateApplication& SlateApp, const FPointerEvent& MouseEvent, const FValueLadderPropertyTarget& Target);
	void EnsureOverlay(FSlateApplication& SlateApp);
	void UpdateOverlay();
	void DestroyOverlay();
	void EndGesture(bool bCommit);
	int32 ResolveActiveLadderIndex(const UValueLadderSettings& Settings, const FVector2D& CursorPosition) const;

	bool bDragging = false;
	FVector2D DragStartPosition = FVector2D::ZeroVector;
	FVector2D OverlayAnchorPosition = FVector2D::ZeroVector;
	FValueLadderPropertyTarget ActiveTarget;
	FValueLadderSession Session;
	TArray<FText> ActiveLadderValues;
	int32 StartLadderIndex = 0;
	int32 ActiveLadderIndex = 0;
	uint64 ActiveGestureId = 0;
	uint64 NextGestureId = 1;

	TSharedPtr<SValueLadderOverlay> OverlayWidget;
	TWeakPtr<SWindow> OverlayWindow;
};
