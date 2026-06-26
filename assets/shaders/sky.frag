#version 410 core

/*
 * sky.frag
 * Calcule la couleur du ciel en fonction de la hauteur du rayon de vue :
 * horizon clair, zénith bleu, et une légère brume colorée sous l'horizon.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

in vec2 v_ndc;
out vec4 frag_color;

uniform mat4 u_invViewProj;
uniform vec3 u_camPos;
uniform vec3 u_sunDir;

void main()
{
	vec4 world = u_invViewProj * vec4(v_ndc, 1.0, 1.0);
	vec3 dir = normalize(world.xyz / world.w - u_camPos);

	//cycle jour nuit
	float isDay = smoothstep(-0.15, 0.15, u_sunDir.y); 

	vec3 dayZenith = vec3(0.12, 0.35, 0.75);
	vec3 dayHorizon = vec3(0.70, 0.85, 0.95);
	vec3 duskHorizon = vec3(1.00, 0.45, 0.15);
	vec3 nightZenith = vec3(0.01, 0.02, 0.05);
	vec3 nightHorizon= vec3(0.05, 0.06, 0.10);
	//horiwon dynamique passe a orange vers le couche de soleil
	vec3 currentHorizon = mix(nightHorizon, mix(duskHorizon, dayHorizon, clamp(u_sunDir.y * 3.0, 0.0, 1.0)), isDay);
	vec3 currentZenith = mix(nightZenith, dayZenith, isDay);
	vec3 color;
	float verticalFade = smoothstep(0.0, 0.5, abs(dir.y));

	if (dir.y >= 0.0)
	{
		//ciel
		color = mix(currentHorizon, currentZenith, verticalFade);
	} else
	{
		// sous la map : on utilise la couleur de l'horizon de la période actuelle
		// pour que la transition soit naturelle entre le ciel et le dessous.
		vec3 abyssBottom = mix(vec3(0.01, 0.01, 0.02), vec3(0.05, 0.05, 0.06), isDay);
		color = mix(currentHorizon, abyssBottom, verticalFade);
	}

	// soleil plus halo
	float sunAlign = max(0.0, dot(dir, u_sunDir));
	
	//halo
	float sunGlow = pow(sunAlign, 16.0) * 0.4 + pow(sunAlign, 64.0) * 0.5;
	float sunCore = pow(sunAlign, 2048.0) * 2.5;

	//scouleur depend de couche de soleil ou pas
	vec3 sunColor = mix(vec3(1.0, 0.25, 0.05), vec3(1.0, 0.9, 0.8), clamp(u_sunDir.y * 5.0, 0.0, 1.0));
	float glowMask = smoothstep(-0.25, 0.05, dir.y);
	float coreMask = smoothstep(-0.02, 0.05, dir.y);
	color += sunColor * sunGlow * glowMask * isDay;
	color += vec3(1.0) * sunCore * coreMask * isDay;

	//Tone mapping
	color = vec3(1.0) - exp(-color * 1.3);

	frag_color = vec4(color, 1.0);
}
