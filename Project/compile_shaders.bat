:: Misc
"tools/glslc.exe" -O -fshader-stage=vertex assets/shaders/vertex.glsl -o assets/shaders/vertex.spv
"tools/glslc.exe" -O -fshader-stage=fragment assets/shaders/fragment.glsl -o assets/shaders/fragment.spv

"tools/glslc.exe" -O -fshader-stage=vertex assets/shaders/skyboxVertex.glsl -o assets/shaders/skyboxVertex.spv
"tools/glslc.exe" -O -fshader-stage=fragment assets/shaders/skyboxFragment.glsl -o assets/shaders/skyboxFragment.spv

"tools/glslc.exe" -O -fshader-stage=vertex assets/shaders/fullscreenVertex.glsl -o assets/shaders/fullscreenVertex.spv

"tools/glslc.exe" -O -fshader-stage=compute assets/shaders/genIntegrationLUTCompute.glsl -o assets/shaders/genIntegrationLUTCompute.spv

:: Deferred
"tools/glslc.exe" -O -fshader-stage=vertex assets/shaders/geometryVertex.glsl -o assets/shaders/geometryVertex.spv
"tools/glslc.exe" -O -fshader-stage=fragment assets/shaders/geometryFragment.glsl -o assets/shaders/geometryFragment.spv

"tools/glslc.exe" -O -fshader-stage=fragment assets/shaders/lightFragment.glsl -o assets/shaders/lightFragment.spv
:: Cube-Map filtering
"tools/glslc.exe" -O -fshader-stage=vertex assets/shaders/filterCubemap.glsl -o assets/shaders/filterCubemap.spv

"tools/glslc.exe" -O -fshader-stage=fragment assets/shaders/genCubemapFragment.glsl -o assets/shaders/genCubemapFragment.spv
"tools/glslc.exe" -O -fshader-stage=fragment assets/shaders/genIntegrationMap.glsl -o assets/shaders/genIntegrationMap.spv
"tools/glslc.exe" -O -fshader-stage=fragment assets/shaders/genIrradianceFragment.glsl -o assets/shaders/genIrradianceFragment.spv
"tools/glslc.exe" -O -fshader-stage=fragment assets/shaders/preFilterEnvironment.glsl -o assets/shaders/preFilterEnvironment.spv
:: Raytracing
"tools/glslc.exe" -O -fshader-stage=rgen assets/shaders/raytracing/raygen.glsl -o assets/shaders/raytracing/raygen.spv
"tools/glslc.exe" -O -fshader-stage=rmiss assets/shaders/raytracing/miss.glsl -o assets/shaders/raytracing/miss.spv
"tools/glslc.exe" -O -fshader-stage=rchit assets/shaders/raytracing/closesthit.glsl -o assets/shaders/raytracing/closesthit.spv
"tools/glslc.exe" -O -fshader-stage=rmiss assets/shaders/raytracing/missShadow.glsl -o assets/shaders/raytracing/missShadow.spv
"tools/glslc.exe" -O -fshader-stage=rchit assets/shaders/raytracing/closesthitShadow.glsl -o assets/shaders/raytracing/closesthitShadow.spv
"tools/glslc.exe" -O -fshader-stage=compute assets/shaders/raytracing/blur.glsl -o assets/shaders/raytracing/blur.spv
:: Particles
"tools/glslc.exe" -O -fshader-stage=vertex assets/shaders/particles/vertex.glsl -o assets/shaders/particles/vertex.spv
"tools/glslc.exe" -O -fshader-stage=fragment assets/shaders/particles/fragment.glsl -o assets/shaders/particles/fragment.spv
"tools/glslc.exe" -O -fshader-stage=compute assets/shaders/particles/update_cs.glsl -o assets/shaders/particles/update_cs.spv

:: Shadow Map
"tools/glslc.exe" -O -fshader-stage=vertex assets/shaders/shadowMapVertex.glsl -o assets/shaders/shadowMapVertex.spv

:: Volumetric lighting
"tools/glslc.exe" -fshader-stage=vertex assets/shaders/volumetricLight/volumetricPointVertex.glsl -o assets/shaders/volumetricLight/volumetricPointVertex.spv
"tools/glslc.exe" -fshader-stage=fragment assets/shaders/volumetricLight/volumetricPointFragment.glsl -o assets/shaders/volumetricLight/volumetricPointFragment.spv

"tools/glslc.exe" -fshader-stage=fragment assets/shaders/volumetricLight/volumetricDirectionalFragment.glsl -o assets/shaders/volumetricLight/volumetricDirectionalFragment.spv

"tools/glslc.exe" -fshader-stage=fragment assets/shaders/volumetricLight/volumetricApplyFragment.glsl -o assets/shaders/volumetricLight/volumetricApplyFragment.spv

pause
