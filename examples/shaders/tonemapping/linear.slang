implementing tonemapper;

public struct LinearToneMapper : IToneMapper
{
  public float3 evaluate(float3 color)
  {
    return color / (color + float3(1));
  }
}
