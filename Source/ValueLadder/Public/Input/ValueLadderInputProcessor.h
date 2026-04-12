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
	void ClearPendingActivation();
	void EnsureOverlay(FSlateApplication& SlateApp, const FVector2D& CursorPosition);
	void UpdateOverlay(const FVector2D& CursorPosition);
	void DestroyOverlay();

	bool bDragging = false;
	bool bPendingActivation = false;
	FVector2D DragStartPosition = FVector2D::ZeroVector;
	FValueLadderPropertyTarget PendingTarget;
	FValueLadderSession Session;

	TSharedPtr<SValueLadderOverlay> OverlayWidget;
	TWeakPtr<SWindow> OverlayWindow;
};
