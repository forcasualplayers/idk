#version 460

layout (input_attachment_index=1, set=2, binding=0) uniform subpassInput metallic_light_accum_input;
layout (input_attachment_index=2, set=2, binding=1) uniform subpassInput metallic_depth_input;
layout (input_attachment_index=3, set=2, binding=2) uniform subpassInput specular_light_accum_input;
layout (input_attachment_index=4, set=2, binding=3) uniform subpassInput specular_depth_input;


layout(location=0) out vec4 out_color;
out float gl_FragDepth;

//(Relative luminance, obtained from wiki)
float Luminance(vec3 color)
{
	return 0.2126*color.r + 0.7152*color.g + 0.0722*color.b;
}

//Tone Mapping
//Formula: Ld(x,y) = L(x,y)/(1+L(x,y)), where Ld(x,y) is the mapped luminance
//since L = 0.2126*color.r + 0.7152*color.g + 0.0722*color.b;
//and Ld(x,y) = L(x,y)/(1+L(x,y))
//Sub L into L(x,y) as the numerator
// Ld(x,y) = (0.2126*color(x,y).r + 0.7152*color(x,y).g + 0.0722*color(x,y).b)/(1 + L(x,y));
//by distributivity: Distribute denominator into the sums
// Ld(x,y) = (0.2126*color(x,y).r/(1 + L(x,y)) + 0.7152*color(x,y).g/(1 + L(x,y)) + 0.0722*color(x,y).b/(1 + L(x,y)));
//we can now collapse color_prime(x,y).component = color(x,y).component/(1+L(x,y))
//this results in:
// Ld(x,y) = (0.2126*color_prime(x,y).r + 0.7152*color_prime(x,y).g + 0.0722*color_prime(x,y).b);
//which means, that the new color we're looking for to achieve the desired luminance is color_prime.
// color_prime(x,y) = color(x,y) / (L(x,y)+1);

vec3 ReinhardOperator(vec3 color)
{
	float L = Luminance(color.rgb); 
	return color/(L+1);
}

void main()
{
	float metallic_depth = subpassLoad(metallic_depth_input).r;
	float specular_depth = subpassLoad(specular_depth_input).r;
	
	float depth =min(metallic_depth,specular_depth);
	if(depth==1)
		discard;
		
	vec3  specular_light = subpassLoad(specular_light_accum_input).rgb;
	vec3  metallic_light = subpassLoad(metallic_light_accum_input).rgb;
	
	vec3 light =(metallic_depth<specular_depth)?metallic_light:specular_light;
	vec3 color = ReinhardOperator(light); 
	out_color = vec4(color,1.f);
	gl_FragDepth = depth;
}