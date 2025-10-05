module channel;

public interface ISampler
{
  public float4 sample(float2 uv);
}

public struct ConstSampler : ISampler
{
  float4 value;
  public float4 sample(float2 uv) { return value; }
}

public struct TextureSampler : ISampler
{
  Texture2D texture;
  SamplerState linear_sampler;
  public float4 sample(float2 uv) { return texture.Sample(linear_sampler, uv); }
}

public struct FactoredTextureSampler : ISampler
{
  float4 factor;
  Texture2D texture;
  SamplerState linear_sampler;
  public float4 sample(float2 uv) { return factor * texture.Sample(linear_sampler, uv); }
}
