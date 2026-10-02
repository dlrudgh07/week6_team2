#pragma once

enum class ERasterizerState : uint8 { SolidBack, SolidNone, SolidFront, Wireframe, Count };
enum class EDepthStencilState : uint8 { Default, ReadOnly, Disabled, StencilMask, StencilOutline, Count };
enum class EBlendState : uint8 { Opaque, AlphaBlend, NoColorWrite, Additive, Count };
enum class ESamplerState : uint8 { LinearClamp, LinearWrap, PointClamp, Count };
