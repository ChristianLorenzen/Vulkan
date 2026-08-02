#version 450
#extension GL_GOOGLE_include_directive:require
// Editor reference grid -- fragment stage.
//
// Pipeline of work per pixel:
//   1. Intersect the interpolated eye ray with the horizontal plane Y = height.
//      Discard when the ray points away from the plane (above the horizon) or
//      when the hit lies behind the near plane / past the far plane.
//   2. Pick two adjacent decades of cell size (1m / 10m / 100m ...) such that
//      the finer of the two is still at least `minPixelsBetweenCells` apart on
//      screen. This is the anti-aliasing strategy: instead of trying to filter a
//      grid that has collapsed below a pixel, we stop drawing it and let the
//      coarser decade take over, cross-fading between the two.
//   3. Compute analytic line coverage using screen-space derivatives of the
//      world-space plane coordinates, so lines stay ~2px wide at any distance.
//   4. Tint the two principal axes, fade with distance, and write gl_FragDepth
//      from the true intersection point so scene geometry occludes the grid
//      correctly.
//
// Blending is alpha-over onto scene colour; the motion-vector attachment is
// masked off by the pipeline, and the zero written here is never consumed.

#include "FayeGlobal.glsl"

layout(location=0)in vec3 vNearPoint;
layout(location=1)in vec3 vFarPoint;

layout(location=0)out vec4 outColor;
layout(location=1)out vec2 outMotion;

layout(push_constant)uniform Push
{
    vec4 thinLineColor;
    vec4 thickLineColor;
    vec4 xAxisColor;
    vec4 zAxisColor;
    float cellSize;
    float minPixelsBetweenCells;
    float maxDistance;
    float planeHeight;
}push;

// Desired on-screen line thickness, in pixels.
const float kLineWidthPixels=2.;

float log10f(float x){return log(x)*.4342944819;}// 1 / ln(10)
float saturatef(float x){return clamp(x,0.,1.);}

// Coverage of the nearest grid line of period `cell`, anti-aliased against the
// per-pixel world-space footprint. Returns 1 on a line centre and falls linearly
// to 0 one footprint away, which is a box filter over the pixel -- cheap, and
// indistinguishable from anything fancier at 2px line widths.
float lineCoverage(vec2 planeUv,vec2 footprint,float cell)
{
    vec2 halfCell=vec2(cell*.5);
    // GLSL mod() is floor-based, so this is correct for negative coordinates.
    vec2 distanceToLine=abs(mod(planeUv+halfCell,vec2(cell))-halfCell);
    vec2 coverage=vec2(1.)-clamp(distanceToLine/max(footprint,vec2(1e-8)),0.,1.);
    return max(coverage.x,coverage.y);
}

void main()
{
    vec3 rayStart=vNearPoint;
    vec3 rayDelta=vFarPoint-vNearPoint;
    
    // --- 1. Ray / plane intersection -------------------------------------
    // A ray parallel to the plane never hits it. Rather than branching out
    // here, clamp the denominator away from zero and reject below: the plane
    // coordinates feed dFdx/dFdy, and derivatives are only well defined when
    // every helper lane in the quad reaches them. Discarding first would leave
    // quads straddling the horizon with garbage footprints -- visible as a band
    // of aliasing exactly where the grid is hardest to filter.
    float denominator=rayDelta.y;
    float safeDenominator=abs(denominator)<1e-6?1e-6:denominator;
    
    float t=(push.planeHeight-rayStart.y)/safeDenominator;
    vec3 hitWorld=rayStart+t*rayDelta;
    vec2 planeUv=hitWorld.xz;
    
    // --- 2. LOD selection -------------------------------------------------
    // World-space size of this pixel's footprint on the plane. Grows without
    // bound toward the horizon, which is exactly what drives the LOD up.
    vec2 footprint=vec2(length(vec2(dFdx(planeUv.x),dFdy(planeUv.x))),
    length(vec2(dFdx(planeUv.y),dFdy(planeUv.y))));
    float footprintLength=max(length(footprint),1e-8);
    
    // Now safe to bail: all derivatives have been taken. `t` is normalised over
    // the near..far segment, so one test rejects both "plane is behind the
    // camera" (above the horizon) and "hit is beyond the far plane".
    if(abs(denominator)<1e-6||t<0.||t>1.)
    discard;
    
    // Continuous decade index. +1 keeps the base cell visible when the camera
    // is close enough that one cell already spans many pixels.
    // max() guards against a zeroed push block (e.g. a UI field dragged to 0)
    // producing a division by zero and a NaN cascade through the whole grid.
    float baseCell=max(push.cellSize,1e-4);
    float lod=max(0.,
        log10f(footprintLength*max(push.minPixelsBetweenCells,.5)/baseCell)+1.);
        float lodBlend=fract(lod);
        
        float cellFine=baseCell*pow(10.,floor(lod));
        float cellMid=cellFine*10.;
        float cellCoarse=cellMid*10.;
        
        // Scale the footprint to the requested line thickness before measuring
        // coverage, so lines stay a constant pixel width at every distance.
        vec2 lineFootprint=footprint*kLineWidthPixels;
        
        float coverageFine=lineCoverage(planeUv,lineFootprint,cellFine);
        float coverageMid=lineCoverage(planeUv,lineFootprint,cellMid);
        float coverageCoarse=lineCoverage(planeUv,lineFootprint,cellCoarse);
        
        // --- 3. Compose the three decades ------------------------------------
        // Coarser lines win where they coincide with finer ones, and the finest
        // decade dissolves as it approaches the pixel-density floor.
        vec4 lineColor;
        float coverage;
        if(coverageCoarse>0.)
        {
            lineColor=push.thickLineColor;
            coverage=coverageCoarse;
        }
        else if(coverageMid>0.)
        {
            lineColor=mix(push.thickLineColor,push.thinLineColor,lodBlend);
            coverage=coverageMid;
        }
        else
        {
            lineColor=push.thinLineColor;
            coverage=coverageFine*(1.-lodBlend);
        }
        
        if(coverage<=0.)
        discard;
        
        // --- 4. Axis tint -----------------------------------------------------
        // The +X axis is the line Z == 0; the +Z axis is the line X == 0. Compare
        // against the same footprint used for the lines so the highlight is exactly
        // as wide as an ordinary grid line.
        if(abs(planeUv.y)<lineFootprint.y)
        lineColor=push.xAxisColor;
        else if(abs(planeUv.x)<lineFootprint.x)
        lineColor=push.zAxisColor;
        
        // --- 5. Distance fade -------------------------------------------------
        // Measured on the plane rather than in 3D so the fade radius is independent
        // of camera altitude. Squared for a softer approach to the horizon.
        float radialDistance=length(planeUv-fayeCameraPosWorld().xz);
        float distanceFade=1.-saturatef(radialDistance/max(push.maxDistance,1e-3));
        
        float alpha=coverage*lineColor.a*distanceFade*distanceFade;
        if(alpha<=0.)
        discard;
        
        // --- 6. Depth ---------------------------------------------------------
        // Reproject the intersection so the depth test resolves the grid against
        // scene geometry. Vulkan's depth range is [0, 1], so the perspective divide
        // gives the value directly. Note this forces late-Z for this draw, which is
        // acceptable for one full-screen triangle at the end of the scene pass.
        vec4 clipPos=ubo.projection*ubo.view*vec4(hitWorld,1.);
        gl_FragDepth=clipPos.z/clipPos.w;
        
        outColor=vec4(lineColor.rgb,alpha);
        outMotion=vec2(0.);
    }
    