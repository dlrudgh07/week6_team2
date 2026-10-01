#pragma once

#include "BillboardComponent.h"
#include "Text/Font.h"

class UTextRenderComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UTextRenderComponent, UPrimitiveComponent)

	REFLECT_START(ClassName)
		PROPERTY(Text)
		PROPERTY(TextSize)
		PROPERTY(Font)
		REFLECT_END()

public:
	UTextRenderComponent();
	virtual ~UTextRenderComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	virtual const FStaticMeshData* GetMeshData() const override;


	// 텍스트는 FTextRenderer가 따로 그리므로 렌더 패킷을 만들지 않음
	virtual void SubmitToRenderPackets(TArray<FRenderPacket>& OutPackets) override {}

	const FString& GetText() const { return Text; }
	void SetText(const FString& InText) { Text = InText; }

	UFont* GetFont() const { return Font; }
	void SetFont(UFont* InFont) { Font = InFont; }

	float GetTextSize() const { return TextSize < 0 ? 0.1f : TextSize; }
	void SetTextSize(float InTextSize) { TextSize = InTextSize; }

private:
	bool RebuildPickingMesh() const;

	FString Text = "Text";
	UFont* Font = nullptr;
	float TextSize = 1.0f;

	mutable FStaticMeshData PickingMesh;
	mutable FString CachedText;
	mutable float CachedTextSize = -1.0f;
	mutable UFont* CachedFont = nullptr;
	mutable bool bHasPickingMesh = false;
};
