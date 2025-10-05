module tonemapper;

__include "aces.slang";
__include "linear.slang";

public interface IToneMapper
{
  public float3 evaluate(float3 color);
}
