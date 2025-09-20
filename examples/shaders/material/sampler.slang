module channel;

public interface ISampler
{
  float4 sample(float2 uv);
}

public struct ConstSampler : ISampler
{
  float4 value;
  float4 sample(float2 uv) { return value; }
}

public struct TextureSampler : ISampler
{
  Texture2D texture;
  SamplerState linear_sampler;
  float4 sample(float2 uv) { return texture.Sample(linear_sampler, uv); }
}

public struct FactoredTextureSampler : ISampler
{
  float4 factor;
  Texture2D texture;
  SamplerState linear_sampler;
  float4 sample(float2 uv) { return factor * texture.Sample(linear_sampler, uv); }
}
