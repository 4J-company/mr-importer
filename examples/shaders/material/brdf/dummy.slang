module dummy;

public struct DummyBRDF
{
  public float3 evaluate(float3 wo, float3 wi, float3 N)
  {
    return float3(30. / 255, 47. / 255, 80. / 255);
  }
}
