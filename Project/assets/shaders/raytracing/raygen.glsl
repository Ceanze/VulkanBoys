#version 460
#extension GL_NV_ray_tracing : require

#include "../helpers.glsl"

layout(binding = 0, set = 0, rgba16f) uniform image2D u_RadianceImage;
layout(binding = 19, set = 0, rgba16f) uniform image2D u_ReflectionImage;
layout(binding = 1, set = 0) uniform CameraProperties 
{
	mat4 viewInverse;
	mat4 projInverse;
} u_Cam;
layout(binding = 2, set = 0) uniform accelerationStructureNV u_TopLevelAS;
layout(binding = 3, set = 0) uniform sampler2D u_Albedo_AO;
layout(binding = 4, set = 0) uniform sampler2D u_Normal_Metallic_Roughness;
layout(binding = 5, set = 0) uniform sampler2D u_Depth;
layout(binding = 17, set = 0) uniform sampler2D u_Velocity;
layout(binding = 18, set = 0) uniform sampler2D u_BrdfLUT;
layout(binding = 20, set = 0) uniform sampler2D u_BlueNoiseLUT;

layout (constant_id = 2) const int MAX_POINT_LIGHTS = 4;

struct PointLight
{
	vec4 Color;
	vec4 Position;
};

layout (binding = 16) uniform LightBuffer
{
	PointLight lights[MAX_POINT_LIGHTS];
} u_Lights;

struct RayPayload 
{
	vec3 Radiance;
	uint Recursion;
};

struct ShadowRayPayload
{
	float occluderFactor;
};

layout (push_constant) uniform PushConstants
{
	float counter;
} u_PushConstants;

layout(location = 0) rayPayloadNV RayPayload rayPayload;
layout(location = 1) rayPayloadNV ShadowRayPayload shadowRayPayload;

void calculatePositions(in vec2 uvCoords, in float z, out vec3 worldPos, out vec3 viewSpacePos)
{
	vec2 xy = uvCoords * 2.0f - 1.0f;

	vec4 clipSpacePosition = vec4(xy, z, 1.0f);
	vec4 viewSpacePosition = u_Cam.projInverse * clipSpacePosition;
	viewSpacePosition = viewSpacePosition / viewSpacePosition.w;
	vec4 homogenousPosition = u_Cam.viewInverse * viewSpacePosition;

	worldPos = homogenousPosition.xyz;
	viewSpacePos = viewSpacePosition.xyz;
}

void calculateDirections(in vec2 uvCoords, in vec3 hitPosition, in vec3 normal, out vec3 reflDir, out vec3 viewDir)
{
	vec4 u_CameraOrigin = u_Cam.viewInverse * vec4(0.0f, 0.0f, 0.0f, 1.0f);
	vec3 origDirection = normalize(hitPosition - u_CameraOrigin.xyz);

	reflDir = reflect(origDirection, normal);
	viewDir = -origDirection;
}

vec3 calculateNormal(vec4 sampledNormalMetallicRoughness)
{
	vec3 normal;
	normal.xy 	= sampledNormalMetallicRoughness.xy;
	normal.z 	= sqrt(1.0f - dot(normal.xy, normal.xy));
	if (sampledNormalMetallicRoughness.a < 0)
	{
		normal.z = -normal.z;
	}
	normal = normalize(normal);
	return normal;
}

void main() 
{
	//Calculate Screen Coords
	const ivec2 pixelCoords = ivec2(gl_LaunchIDNV.xy);
	const vec2 pixelCenter = vec2(pixelCoords) + vec2(0.5f);
	vec2 uvCoords = (pixelCenter / vec2(gl_LaunchSizeNV.xy));

	//Sample GBuffer
	vec4 sampledNormalMetallicRoughness = texture(u_Normal_Metallic_Roughness, uvCoords);
	vec3 normal = calculateNormal(sampledNormalMetallicRoughness);

	//Skybox
	if (dot(normal, normal) < 0.5f)
	{
		imageStore(u_RadianceImage, pixelCoords, vec4(1.0f, 0.0f, 1.0f, 0.0f));
		imageStore(u_ReflectionImage, pixelCoords, vec4(1.0f, 0.0f, 1.0f, 0.0f));
		return;
	}

	//Sample GBuffer
	vec4 sampledAlbedoAO = texture(u_Albedo_AO, uvCoords);
	float sampledDepth = texture(u_Depth, uvCoords).r;

	//Define Constants
	vec3 hitPos = vec3(0.0f);
	vec3 viewSpacePos = vec3(0.0f);
	calculatePositions(uvCoords, sampledDepth, hitPos, viewSpacePos);

	//Define new Rays Parameters
	vec3 reflectedRaysOrigin = hitPos + normal * EPSILON;
	uint rayFlags = gl_RayFlagsOpaqueNV;
	uint cullMask = 0xff;
	float tmin = 0.001f;
	float tmax = 10000.0f;

	vec3 reflDir = vec3(0.0f);
	vec3 viewDir = vec3(0.0f);
	calculateDirections(uvCoords, hitPos, normal, reflDir, viewDir);

	//Setup PBR Parameters
	vec3 albedo = sampledAlbedoAO.rgb;
	float ao = sampledAlbedoAO.a;
	float roughness = sampledNormalMetallicRoughness.a;
	float metallic = sampledNormalMetallicRoughness.z;

	//Init BRDF values
	vec3 f0 = vec3(0.04f);
	f0 = mix(f0, albedo, metallic);

	float metallicFactor = 1.0f - metallic;
	
	float NdotV = max(dot(viewDir, normal), 0.0f);

	vec3 Lo = vec3(0.0f);
	for (int i = 0; i < MAX_POINT_LIGHTS; i++)
	{
		vec3 lightPosition 	= u_Lights.lights[i].Position.xyz;
		vec3 lightColor 	= u_Lights.lights[i].Color.rgb;

		vec3 lightVector 	= (lightPosition - hitPos);
		vec3 lightDir 		= normalize(lightVector);

		traceNV(u_TopLevelAS, rayFlags, cullMask, 1, 0, 1, reflectedRaysOrigin, tmin, lightDir, tmax, 1);

		if (shadowRayPayload.occluderFactor < 0.1f)
		{
			float lightDistance	= length(lightVector);
			float attenuation 	= 1.0f / (lightDistance * lightDistance);

			vec3 halfVector = normalize(viewDir + lightDir);

			vec3 radiance = lightColor * attenuation;

			float HdotV = max(dot(halfVector, viewDir), 0.0f);
			float NdotL = max(dot(normal, lightDir), 0.0f);

			vec3 f 		= Fresnel(f0, HdotV);
			float ndf 	= Distribution(normal, halfVector, roughness);
			float g 	= GeometryOpt(NdotV, NdotL, roughness);

			float denom 	= 4.0f * NdotV * NdotL + 0.0001f;
			vec3 specular   = (ndf * g * f) / denom;

			//Take 1.0f minus the incoming radiance to get the diffuse (Energy conservation)
			vec3 diffuse = (vec3(1.0f) - f) * metallicFactor;

			Lo += ((diffuse * (albedo / PI)) + specular) * radiance * NdotL;
		}
	}

	imageStore(u_RadianceImage, pixelCoords, vec4(Lo, 1.0f));

	//Reflection
	vec2 uniformRandom = texture(u_BlueNoiseLUT, uvCoords + vec2(u_PushConstants.counter)).rg;

	vec3 Rt = vec3(0.0f);
	vec3 Rb = vec3(0.0f);
	createCoordinateSystem(reflDir, Rt, Rb);

	reflDir = ReflectanceDirection2(reflDir, Rt, Rb, roughness, uniformRandom);
	rayPayload.Radiance = vec3(0.0f);
	rayPayload.Recursion = 0;
	traceNV(u_TopLevelAS, rayFlags, cullMask, 0, 0, 0, reflectedRaysOrigin, tmin, reflDir, tmax, 0);


	//Temporal Filtering
	//vec4 motion = texelFetch(TEX_PT_MOTION, ipos, 0);
	vec4 motion = texture(u_Velocity, uvCoords);

	ivec2 currentReflectionDimensions = imageSize(u_ReflectionImage); 
	ivec2 prevReflectionDimensions = currentReflectionDimensions; //Todo: Fix this?

	vec2 currentScreenCoords = pixelCoords;
	vec2 prevScreenCoords = (uvCoords + motion.xy) * prevReflectionDimensions;

	bool temporal_sample_valid = false;
	vec4 temporal_color_histlen_spec = vec4(0);

	float temporalSumSpecularWeight = 0.0f;

	vec2 prevFlooredScreenCoords = floor(prevScreenCoords - vec2(0.5f));
	vec2 prevSubpixel = fract(prevScreenCoords - vec2(0.5f) - prevFlooredScreenCoords);

	// Bilinear/bilateral filter
	const ivec2 off[4] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };
	float w[4] = 
	{
		(1.0f - prevSubpixel.x) * (1.0f - prevSubpixel.y),
		(prevSubpixel.x       ) * (1.0f - prevSubpixel.y),
		(1.0f - prevSubpixel.x) * (prevSubpixel.y       ),
		(prevSubpixel.x       ) * (prevSubpixel.y       )
	};

	bool temp_bool = false;

	for(int i = 0; i < 4; i++) 
	{
		ivec2 p = ivec2(prevFlooredScreenCoords) + off[i];
		vec2 previousUVCoords = (vec2(p) + 0.5f) / prevReflectionDimensions;

		if(p.x < 0.0f || p.x >= prevReflectionDimensions.x || p.y  < 0.0f || p.y >= prevReflectionDimensions.y)
			continue;

		float previousDepth = texture(u_Depth, previousUVCoords).x;
		vec3 previousNormal = calculateNormal(texture(u_Normal_Metallic_Roughness, previousUVCoords));

		float depthDistance = abs(motion.z) / 2.0f; //Average Delta Depth
		float CNdotPN = dot(normal, previousNormal);

		if(depthDistance < 0.5f && CNdotPN > 0.5f) 
		{
			float wDiff = w[i];
			float wSpec = wDiff * pow(max(CNdotPN, 0.0f), 128.0f);

			temporal_color_histlen_spec   += imageLoad(u_ReflectionImage, p) * wSpec;
			temporalSumSpecularWeight     += wSpec;
			temp_bool = true;
		}
	}

	// We found some relevant surface - good
	if(temporalSumSpecularWeight > 0.000001f)
	{
		float invWSpec = 1.0f / temporalSumSpecularWeight;
		temporal_color_histlen_spec   *= invWSpec;
		temporal_sample_valid         = true;
	}
	
	vec3 color_curr_spec = rayPayload.Radiance;
	
	vec4 out_color_histlen_spec;

	float flt_min_alpha_color_spec = 0.0625f;

	if(temporal_sample_valid && temp_bool)
	{
		float hist_len_spec = clamp(temporal_color_histlen_spec.a + 1.0f, 1.0f, 128.0f);

		// Compute the blending weights based on history length, so that the filter
		// converges faster. I.e. the first frame has weight of 1.0, the second frame 1/2, third 1/3 and so on.
		float alpha_color_spec = max(flt_min_alpha_color_spec, 1.0f / hist_len_spec);
		//float alpha_color_spec = 0.25f;
		
	   	out_color_histlen_spec.rgb = mix(temporal_color_histlen_spec.rgb, color_curr_spec.rgb, alpha_color_spec);
		//out_color_histlen_spec.rgb = temporal_color_histlen_spec.rgb;

		out_color_histlen_spec.a = hist_len_spec;
		//out_color_histlen_spec.a = 1.0f;
	}
	else
	{
		// No valid history - just use the current color and spatial moments
		out_color_histlen_spec = vec4(color_curr_spec, 1.0f);
	}

	imageStore(u_ReflectionImage, pixelCoords, out_color_histlen_spec);
}