struct VertexInput
{
  float3 position : POSITION;
  float3 normal : NORMAL;
  float2 texCoord : TEXCOORD0;
};

struct VertexOutput
{
  float4 position : SV_Position;
  float3 normal : NORMAL;
  float2 texCoord : TEXCOORD0;
  float3 worldPos : TEXCOORD1;
};

struct TransformData
{
  float4x4 modelMatrix;
  float4x4 viewMatrix;
  float4x4 projectionMatrix;
  float4x4 modelViewProjectionMatrix;
};

[[vk::binding(0, 0)]]
ConstantBuffer<TransformData> transformData;

struct MaterialData
{
  float4 diffuseColor;
  float4 specularColor;
  float shininess;
  float specularIntensity;
};

[[vk::binding(1, 0)]]
ConstantBuffer<MaterialData> materialData;

struct LightData
{
  float4 position;
  float4 color;
  float intensity;
  float3 attenuation;
};

[[vk::binding(2, 0)]]
ConstantBuffer<LightData> lightData;

[[vk::binding(3, 0)]]
Texture2D diffuseTexture;

[[vk::binding(4, 0)]]
SamplerState linearSampler;

VertexOutput vertex_main(VertexInput input)
{
  VertexOutput output;

  float4 worldPos = mul(transformData.modelMatrix, float4(input.position, 1.0));
  output.position = mul(transformData.projectionMatrix, 
      mul(transformData.viewMatrix, worldPos));

  float3x3 normalMatrix = (float3x3)transformData.modelMatrix;
  output.normal = normalize(mul(normalMatrix, input.normal));

  output.texCoord = input.texCoord;

  output.worldPos = worldPos.xyz;

  return output;
}

float4 fragment_main(VertexOutput input) : SV_Target
{
  float3 normal = normalize(input.normal);

  float3 lightDir = normalize(lightData.position.xyz - input.worldPos);

  float diffuseFactor = max(dot(normal, lightDir), 0.0);
  float3 diffuseColor = materialData.diffuseColor.rgb * diffuseFactor * lightData.color.rgb;

  float3 viewDir = normalize(-input.worldPos);
  float3 halfDir = normalize(lightDir + viewDir);
  float specularFactor = pow(max(dot(normal, halfDir), 0.0), materialData.shininess);
  float3 specularColor = materialData.specularColor.rgb * specularFactor * 
    materialData.specularIntensity * lightData.color.rgb;

  float distance = length(lightData.position.xyz - input.worldPos);
  float attenuation = 1.0 / (1.0 + lightData.attenuation.x * distance + 
      lightData.attenuation.y * distance * distance);

  float3 finalColor = (diffuseColor + specularColor) * attenuation * lightData.intensity;

  finalColor += materialData.diffuseColor.rgb * 0.1;

  return float4(finalColor, materialData.diffuseColor.a);
}
