module gammacorrector;

__include "linear.slang";

public interface IGammaCorrector
{
  float3 evaluate(float3 color);
}
