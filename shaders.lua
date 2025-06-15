local shaders = {}

-- Background shader code
function shaders.getBackgroundShaderCode()
  return [[
    uniform float time;
    uniform vec2 resolution;

    vec4 effect(vec4 color, Image texture, vec2 texture_coords, vec2 screen_coords) {
      vec2 uv = screen_coords / resolution;

      // Create a gradient from dark blue at top to darker at bottom
      vec3 skyColor = mix(vec3(0.1, 0.1, 0.3), vec3(0.05, 0.05, 0.15), uv.y);

      // Add some moving clouds/mist
      float cloud1 = sin(uv.x * 6.0 + time * 0.5) * 0.5 + 0.5;
      float cloud2 = sin(uv.x * 4.0 - time * 0.3) * 0.5 + 0.5;
      float cloudPattern = (cloud1 * cloud2) * 0.1;

      // Add subtle stars
      vec2 starUV = uv * 20.0;
      float stars = 0.0;

      // Create a pseudo-random star pattern
      for(int i = 0; i < 3; i++) {
        vec2 offset = vec2(float(i) * 13.7, float(i) * 7.3);
        vec2 starPos = floor(starUV + offset);
        float random = fract(sin(dot(starPos, vec2(12.9898, 78.233))) * 43758.5453);

        if(random > 0.98) {
          vec2 localUV = fract(starUV + offset);
          float dist = distance(localUV, vec2(0.5));
          float twinkle = sin(time * 3.0 + random * 6.28) * 0.5 + 0.5;
          stars += (1.0 - smoothstep(0.0, 0.1, dist)) * twinkle * 0.3;
        }
      }

      // Add subtle vertical movement to create atmosphere
      float atmosphere = sin(uv.y * 3.0 + time * 0.2) * 0.02;

      // Combine all effects
      vec3 finalColor = skyColor + cloudPattern + stars + atmosphere;

      return vec4(finalColor, 1.0);
    }
  ]]
end

-- Initialize background shader with error handling
function shaders.initBackgroundShader()
  local shaderCode = shaders.getBackgroundShaderCode()
  local success, shader = pcall(love.graphics.newShader, shaderCode)

  if success then
    print("Background shader loaded successfully")
    return shader
  else
    print("Warning: Failed to load background shader - " .. tostring(shader))
    return nil
  end
end

return shaders
