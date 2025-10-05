module material;

import sampler;

public interface IMaterial
{
}

public struct BlinnPhongMaterial< TAmbient:ISampler
                                , TDiffuse:ISampler
                                , TSpecular:ISampler
                                , TShininess:ISampler
                                > : IMaterial
{
  TAmbient   ambient;
  TDiffuse   diffuse;
  TSpecular  specular;
  TShininess shininess;
}

public struct StandardPBRMaterial< TBaseColor:ISampler
                                 , TMetallicRoughness:ISampler
                                 , Emissive:ISampler
                                 , TOcclusion:ISampler
                                 > : IMaterial
{
  TBaseColor base_color;
  TMetallicRoughness metallic_roughness;
  Emissive emissive;
  TOcclusion occlusion;
}
