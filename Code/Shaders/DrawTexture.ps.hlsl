Texture2D srcTex;
SamplerState srcSampler;

float4 main(in float4 position : SV_POSITION, in float2 texcoord : TEXCOORD0) : SV_TARGET {
	return srcTex.Sample(srcSampler, texcoord);
}
