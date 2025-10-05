module standardpbr;

#define PI 3.14159265359

public struct StandardPBRPointLight
{
    float3 position;
    float3 color;
    float intensity;
}

public struct StandardPBRMaterialSample
{
    float3 albedo;
    float metallic;
    float roughness;
    float3 emissive;
    float occlusion;
}

float distribution_ggx(float3 n, float3 h, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float n_dot_h = max(dot(n, h), 0.0);
    float n_dot_h2 = n_dot_h * n_dot_h;

    float nom = a2;
    float denom = (n_dot_h2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0001);
}

float geometry_schlick_ggx(float n_dot_v, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float nom = n_dot_v;
    float denom = n_dot_v * (1.0 - k) + k;
    return nom / max(denom, 0.0001);
}

float geometry_smith(float3 n, float3 v, float3 l, float roughness)
{
    float n_dot_v = max(dot(n, v), 0.0);
    float n_dot_l = max(dot(n, l), 0.0);
    float ggx1 = geometry_schlick_ggx(n_dot_v, roughness);
    float ggx2 = geometry_schlick_ggx(n_dot_l, roughness);
    return ggx1 * ggx2;
}

float3 fresnel_schlick(float cos_theta, float3 f0)
{
    return f0 + (1.0 - f0) * pow(max(1.0 - cos_theta, 0.0), 5.0);
}

public struct StandardPBRBRDF
{
    public float3 evaluate(
        float3 camera_position,
        float3 world_position,
        float3 normal,
        StandardPBRPointLight light,
        StandardPBRMaterialSample material)
    {
        float3 n = normalize(normal);
        float3 v = normalize(camera_position - world_position);
        float3 l = normalize(light.position - world_position);
        float3 h = normalize(v + l);

        float distance = length(light.position - world_position);
        float attenuation = 1.0 / (distance * distance);
        float3 radiance = light.color * light.intensity * attenuation;

        float3 f0 = float3(0.04);
        f0 = lerp(f0, material.albedo, material.metallic);

        float ndf = distribution_ggx(n, h, material.roughness);
        float g = geometry_smith(n, v, l, material.roughness);
        float3 f = fresnel_schlick(max(dot(h, v), 0.0), f0);

        float3 numerator = ndf * g * f;
        float denominator = 4.0 * max(dot(n, v), 0.0) * max(dot(n, l), 0.0) + 0.0001;
        float3 specular = numerator / denominator;

        float3 k_s = f;
        float3 k_d = (1.0 - k_s) * (1.0 - material.metallic);

        float n_dot_l = max(dot(n, l), 0.0);
        float3 lo = (k_d * material.albedo / PI + specular) * radiance * n_dot_l;

        float3 ambient = float3(0.03) * material.albedo * material.occlusion;
        float3 color = ambient + lo + material.emissive;

        return color;
    }
}
