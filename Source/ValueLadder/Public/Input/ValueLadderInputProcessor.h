#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "Layout/WidgetPath.h"

#include "Session/ValueLadderSession.h"

class SValueLadderOverlay;
class SWidget;
class SWindow;

class FValueLadderInputProcessor : public IInputProcessor
{
public:
	FValueLadderInputProcessor();
	void CancelActiveGesture();

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;

private:
	bool InitializeGesture(const UValueLadderSettings& Settings, FSlateApplication& SlateApp, const FPointerEvent& MouseEvent, const FWidgetPath& WidgetsUnderCursor, const FValueLadderPropertyTarget& Target);
	void EnsureOverlay(FSlateApplication& SlateApp);
	void UpdateOverlay();
	void DestroyOverlay();
	void EndGesture(bool bCommit, const TCHAR* Reason = TEXT("Unknown"));
	void ResetGestureState();
	int32 ResolveActiveLadderIndex(const UValueLadderSettings& Settings) const;
	bool BeginSlateCapture(FSlateApplication& SlateApp);
	void EndSlateCapture(FSlateApplication& SlateApp);
	bool HasOwnedCapture(FSlateApplication& SlateApp) const;
	FWidgetPath BuildCapturePath() const;

	bool bDragging = false;
	bool bCursorLocked = false;
	bool bPendingCursorRestore = false;
	FVector2D DragStartPosition = FVector2D::ZeroVector;
	FVector2D CursorRestorePosition = FVector2D::ZeroVector;
	FVector2D AccumulatedDragDelta = FVector2D::ZeroVector;
	FVector2D OverlayAnchorPosition = FVector2D::ZeroVector;
	FWidgetPath GestureEventPath;
	TWeakPtr<SWidget> CaptureWidget;
	int32 ActivePointerIndex = 10;
	int32 ActiveUserIndex = 0;
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
