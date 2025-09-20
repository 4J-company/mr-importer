module blinnphong;

public struct BlinnPhongPointLight
{
  float3 position;
  float3 color;
}

public struct BlinnPhongMaterialSample
{
  float3 ambient;
  float3 diffuse;
  float3 specular;
  float  shininess;
}

public struct BlinnPhongBRDF
{
  public float3 evaluate(
    float3 camera_position,
    float3 point_position,
    float3 point_normal,
    BlinnPhongPointLight light,
    BlinnPhongMaterialSample sample)
  {
    float3 light_direction = normalize(light.position - point_position);
    float3 view_direction  = normalize(camera_position - point_position);

    float3 normal = normalize(faceforward(point_normal, view_direction, point_normal));

    float diffuse_intensity = max(0.0, dot(normal, light_direction));

    float3 halfway = normalize(light_direction + view_direction);
    float specular_angle_cos = max(0.0, dot(halfway, normal));
    float specular_intensity = pow(specular_angle_cos, sample.shininess);

    float3 output =
      sample.ambient +
      light.color * sample.diffuse  * diffuse_intensity  +
      light.color * sample.specular * specular_intensity
      ;
    return output;
  }
}
