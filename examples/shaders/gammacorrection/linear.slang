implementing gammacorrector;

public struct LinearGammaCorrector : IGammaCorrector
{
  public float3 evaluate(float3 color)
  {
    return pow(color, float3(1.0 / 2.2));
  }
}
