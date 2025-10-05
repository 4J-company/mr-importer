implementing tonemapper;

public struct ACESToneMapper : IToneMapper
{
  public float3 evaluate(float3 color)
  {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp(
        (color * (a * color + b)) /
        (color * (c * color + d) + e),
        0.f, 1.f);
  }
}
