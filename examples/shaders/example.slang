module example;

import gammacorrection.gammacorrector;
import tonemapping.tonemapper;
import material.material;
import material.brdf.dummy;

[numthreads(1,1,1)]
void computeMain(float3* output)
{
  DummyBRDF dummy_brdf = {};
  LinearToneMapper linear_tone_mapper = {};
  LinearGammaCorrector linear_gamma_corrector = {};

  float3 color = dummy_brdf.evaluate(0, 0, 0);
  color = linear_tone_mapper.evaluate(color);
  color = linear_gamma_corrector.evaluate(color);
  output[0] = color;
}
