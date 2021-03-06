/*
	Unlit colour effect
*/

#include "common.fxh"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

cbuffer SceneParams : register(b0)
{
	SScene scene;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct VSinput
{
	float4 pos : POSITION;
	float4 colour : COLOUR;
};

struct PSinput
{
	float4 pos : SV_POSITION;
	float4 colour : COLOUR;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PSinput VS(VSinput input)
{
	PSinput output = (PSinput)0;
	
	float scale = 1.0f;
	
	matrix M = matrix(
		scale, 0, 0, scene.lightPos.x,
		0, scale, 0, scene.lightPos.y,
		0, 0, scale, scene.lightPos.z,
		0, 0, 0, 1
	);
	
	M = transpose(M);
	
	input.pos.w = 1;
	
	output.pos = mul(input.pos, M);
	output.pos = mul(output.pos, scene.view);
	output.pos = mul(output.pos, scene.projection);

	//output.colour = input.colour;
	output.colour = scene.lightColour;

	return output;
}

float4 PS(PSinput input) : SV_TARGET
{
	return float4(input.colour.rgb, 1.0f);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////