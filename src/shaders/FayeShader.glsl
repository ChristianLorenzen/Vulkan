// FayeShader.glsl - Unity-like material property macros for GLSL shaders.
//
// Usage pattern:
//
//   FAYE_PROPERTIES_BEGIN
//     FAYE_FLOAT(metallic,   0.0)
//     FAYE_FLOAT(roughness,  1.0)
//     FAYE_VEC4(baseColor,   vec4(1.0))
//     FAYE_VEC3(emissive,    vec3(0.0))
//   FAYE_PROPERTIES_END
//
//   FAYE_SAMPLER2D(albedoMap, 1, 0)   // (name, set, binding)
//
//   // In main():
//   vec4 color  = SAMPLE(albedoMap, uv);
//   float metal = PROP(metallic);
//
// The FAYE_PROPERTIES_BEGIN / FAYE_PROPERTIES_END pair declares a global
// struct variable named `fayeProps`.  Default values in the macros are
// documentation-only (GLSL struct members cannot have initialisers); actual
// GPU-side values require a per-material UBO to be allocated and bound
// -- see MaterialTemplate / MaterialTemplateRegistry for the planned
// runtime integration.
//
// The C++ ShaderReflection system reads the compiled SPIRV at load time and
// populates a ShaderReflectionData with the discovered property names, types,
// offsets, and bindings, enabling editor tooling and runtime property overrides
// via MaterialPropertyBlock.

#ifndef FAYE_SHADER_GLSL
#define FAYE_SHADER_GLSL

// ----- Property block macros -----------------------------------------------

// Opens the FayeProperties struct declaration.
#define FAYE_PROPERTIES_BEGIN  struct FayeProperties {

// Closes the struct and declares the global instance `fayeProps`.
#define FAYE_PROPERTIES_END    } fayeProps;

// Scalar / vector property members (default values are comments only).
#define FAYE_FLOAT(name, def)  float name;
#define FAYE_VEC2(name,  def)  vec2  name;
#define FAYE_VEC3(name,  def)  vec3  name;
#define FAYE_VEC4(name,  def)  vec4  name;
#define FAYE_INT(name,   def)  int   name;

// Access a property by name through the global struct instance.
#define PROP(name)  fayeProps.name

// ----- Sampler macros -------------------------------------------------------

// Declares a combined image sampler at the given descriptor set and binding.
// Usage: FAYE_SAMPLER2D(albedoMap, 1, 0)
#define FAYE_SAMPLER2D(name, s, b)  layout(set = s, binding = b) uniform sampler2D name;

// Sample a texture declared with FAYE_SAMPLER2D.
#define SAMPLE(name, uv)  texture(name, uv)

#endif // FAYE_SHADER_GLSL
