#pragma once

struct FFogSceneData
{
	float FogDensity;       
	float FogHeightFalloff; 
	float FogMaxOpacity; 
	float StartDistance;
	float EndDistance;
	float FogCutoffDistance;
	float FogHeight;
	float FogInscatteringColor[3]; 

	bool IsValid() const { return (FogType != EFogType::None);}
	
enum EFogType
{
	None,
	BasicInscatteringFog,
	ExponentialHeightFog,
	DirectionInscatteringFog,
	InscatteingCubemapFog,
	VolumetricFog,
} FogType=EFogType::None;

};
