local particles = {}

-- Particle system storage
local particleSystems = {}

-- Create blood particle system
function particles.createBloodParticles(x, y)
  -- Create blood texture (bigger for more visibility)
  local bloodTexture = love.graphics.newCanvas(3, 3)
  love.graphics.setCanvas(bloodTexture)
  love.graphics.setColor(1.0, 0.0, 0.0, 1) -- Bright red
  love.graphics.rectangle("fill", 0, 0, 3, 3)
  love.graphics.setCanvas()

  -- Create particle system for massive blood explosion
  local psystem = love.graphics.newParticleSystem(bloodTexture, 60)

  -- Configure blood particles - massive single burst
  psystem:setParticleLifetime(0.8, 1.2)      -- Longer lifetime to see the blood properly
  psystem:setEmissionRate(100)               -- Rate doesn't matter since we emit once
  psystem:setEmissionArea("uniform", 15, 15) -- Larger area for big explosion

  -- Initial velocity - particles fly outward in massive explosion
  psystem:setSpeed(100, 400)     -- Wide speed range for dramatic spread
  psystem:setDirection(0)        -- Base direction (0 = right)
  psystem:setSpread(math.pi * 2) -- Full 360 degrees explosion

  -- Gravity and physics
  psystem:setLinearAcceleration(0, 400, 0, 600) -- Stronger gravity
  psystem:setLinearDamping(3, 5)                -- More air resistance

  -- Size and scaling
  psystem:setSizes(1.0, 1.5, 0.1) -- Start bigger, then quickly shrink
  psystem:setSizeVariation(0.7)   -- More size variation

  -- Color and fading - keep red longer
  psystem:setColors(
    1.0, 0.0, 0.0, 1.0, -- Bright red at start
    0.9, 0.1, 0.1, 0.9, -- Stay red in middle
    0.6, 0.0, 0.0, 0.4  -- Dark red but still visible at end
  )

  -- Rotation
  psystem:setRotation(0, math.pi * 2) -- Random rotation
  psystem:setSpin(-4, 4)              -- Faster spinning

  -- Position the particle system
  psystem:setPosition(x, y)

  -- Emit MASSIVE single burst of particles - this is the only emission!
  psystem:emit(50)

  -- STOP any further emissions immediately!
  psystem:setEmissionRate(0)

  -- Store particle system with enough time for blood explosion to finish
  table.insert(particleSystems, {
    system = psystem,
    active = true,
    timer = 0,
    maxTime = 1.5 -- Longer time for blood explosion to be visible
  })
end

-- Create impact/dust particles (for other effects)
function particles.createImpactParticles(x, y, color)
  color = color or { 0.7, 0.7, 0.7, 1 } -- Default gray

  -- Create dust texture
  local dustTexture = love.graphics.newCanvas(1, 1)
  love.graphics.setCanvas(dustTexture)
  love.graphics.setColor(color[1], color[2], color[3], color[4])
  love.graphics.rectangle("fill", 0, 0, 1, 1)
  love.graphics.setCanvas()

  -- Create particle system
  local psystem = love.graphics.newParticleSystem(dustTexture, 20)

  -- Configure dust particles
  psystem:setParticleLifetime(0.5, 1.5)
  psystem:setEmissionRate(100)
  psystem:setEmissionArea("uniform", 5, 5)

  psystem:setSpeed(20, 60)
  psystem:setDirection(-math.pi / 2)            -- Upward
  psystem:setSpread(math.pi / 3)                -- 60 degree spread

  psystem:setLinearAcceleration(0, 100, 0, 200) -- Light gravity
  psystem:setLinearDamping(1, 3)

  psystem:setSizes(0.5, 1.0, 0.1)
  psystem:setSizeVariation(0.3)

  psystem:setColors(
    color[1], color[2], color[3], 0.8,
    color[1], color[2], color[3], 0.4,
    color[1], color[2], color[3], 0.0
  )

  psystem:setPosition(x, y)
  psystem:emit(15)

  table.insert(particleSystems, {
    system = psystem,
    active = true,
    timer = 0,
    maxTime = 2.0
  })
end

-- Create player death particles (body parts falling apart)
function particles.createPlayerDeathParticles(x, y)
  -- Create different colored pieces representing body parts
  local bodyParts = {
    { color = { 0.9, 0.7, 0.5, 1 }, size = 8,  name = "head" }, -- Head (skin tone)
    { color = { 0.8, 0.2, 0.2, 1 }, size = 6,  name = "hat" },  -- Hat (red)
    { color = { 0.2, 0.4, 0.8, 1 }, size = 10, name = "body" }, -- Body (blue shirt)
    { color = { 0.9, 0.7, 0.5, 1 }, size = 4,  name = "arm1" }, -- Left arm (skin)
    { color = { 0.9, 0.7, 0.5, 1 }, size = 4,  name = "arm2" }, -- Right arm (skin)
    { color = { 0.6, 0.3, 0.1, 1 }, size = 4,  name = "leg1" }, -- Left leg (brown pants)
    { color = { 0.6, 0.3, 0.1, 1 }, size = 4,  name = "leg2" }, -- Right leg (brown pants)
  }

  for i, part in ipairs(bodyParts) do
    -- Create texture for this body part
    local partTexture = love.graphics.newCanvas(part.size, part.size)
    love.graphics.setCanvas(partTexture)
    love.graphics.setColor(part.color)
    love.graphics.rectangle("fill", 0, 0, part.size, part.size)
    love.graphics.setCanvas()

    -- Create particle system for this body part
    local psystem = love.graphics.newParticleSystem(partTexture, 1)

    -- Configure each body part with unique physics
    psystem:setParticleLifetime(2.0, 2.5) -- Shorter lifetime since no waiting
    psystem:setEmissionRate(1)

    -- Different starting positions for each body part (relative to player center)
    local offsetX, offsetY = 0, 0
    if part.name == "head" then
      offsetX, offsetY = 0, -15
    elseif part.name == "hat" then
      offsetX, offsetY = 0, -20
    elseif part.name == "body" then
      offsetX, offsetY = 0, 0
    elseif part.name == "arm1" then
      offsetX, offsetY = -8, -5
    elseif part.name == "arm2" then
      offsetX, offsetY = 8, -5
    elseif part.name == "leg1" then
      offsetX, offsetY = -3, 10
    elseif part.name == "leg2" then
      offsetX, offsetY = 3, 10
    end

    -- Set initial velocity - different for each part to create scattered effect
    local initialSpeedX = (math.random() - 0.5) * 200 + offsetX * 4 -- Random horizontal + offset bias
    local initialSpeedY = math.random(-120, -40)                    -- More dramatic upward initial velocity

    psystem:setSpeed(math.sqrt(initialSpeedX * initialSpeedX + initialSpeedY * initialSpeedY) * 0.8)
    psystem:setDirection(math.atan(initialSpeedY, initialSpeedX))
    psystem:setSpread(0.3) -- Slightly larger spread for more chaos

    -- Gravity effect
    psystem:setLinearAcceleration(0, 200, 0, 400) -- Gravity pulls down

    -- Size and visual properties
    psystem:setSizes(1.0, 1.0, 0.8) -- Slightly shrink over time
    psystem:setSizeVariation(0)

    -- Color fading
    psystem:setColors(
      part.color[1], part.color[2], part.color[3], 1.0, -- Full opacity at start
      part.color[1], part.color[2], part.color[3], 0.8, -- Slight fade in middle
      part.color[1], part.color[2], part.color[3], 0.0  -- Fully transparent at end
    )

    -- Rotation - each part spins as it falls
    psystem:setRotation(0, math.pi * 2)
    psystem:setSpin(-3, 3)

    -- Position the particle system at body part location
    psystem:setPosition(x + offsetX, y + offsetY)

    -- Emit single particle for this body part
    psystem:emit(1)

    -- Store particle system
    table.insert(particleSystems, {
      system = psystem,
      active = true,
      timer = 0,
      maxTime = 3.0 -- Keep alive for 3 seconds
    })
  end
end

-- Create water splash particle system
function particles.createWaterSplash(x, y)
  -- Create water droplet texture
  local dropletTexture = love.graphics.newCanvas(2, 2)
  love.graphics.setCanvas(dropletTexture)
  love.graphics.setColor(0.3, 0.6, 1.0, 1) -- Water blue
  love.graphics.rectangle("fill", 0, 0, 2, 2)
  love.graphics.setCanvas()

  -- Create particle system for water splash
  local psystem = love.graphics.newParticleSystem(dropletTexture, 30)

  -- Configure water splash particles
  psystem:setParticleLifetime(0.3, 0.8)    -- Medium lifetime
  psystem:setEmissionRate(50)              -- Rate doesn't matter since we emit once
  psystem:setEmissionArea("uniform", 8, 4) -- Splash area

  -- Initial velocity - particles fly upward and outward like a splash
  psystem:setSpeed(50, 150)          -- Medium speed for realistic splash
  psystem:setDirection(-math.pi / 2) -- Base direction (upward)
  psystem:setSpread(math.pi / 3)     -- 60 degree spread

  -- Gravity and physics
  psystem:setLinearAcceleration(0, 300, 0, 500) -- Gravity effect
  psystem:setLinearDamping(1, 3)                -- Air resistance

  -- Size and scaling
  psystem:setSizes(0.8, 1.2, 0.1) -- Start medium, then shrink
  psystem:setSizeVariation(0.5)   -- Size variation

  -- Color and fading - water effect
  psystem:setColors(
    0.4, 0.7, 1.0, 1.0, -- Light blue at start
    0.3, 0.6, 0.9, 0.8, -- Medium blue in middle
    0.2, 0.4, 0.7, 0.0  -- Dark blue and transparent at end
  )

  -- Rotation
  psystem:setRotation(0, math.pi) -- Some rotation
  psystem:setSpin(-2, 2)          -- Gentle spinning

  -- Position the particle system
  psystem:setPosition(x, y)

  -- Emit single burst of particles
  psystem:emit(25)

  -- Add to our particle systems list
  table.insert(particleSystems, {
    system = psystem,
    timer = 0,
    maxTime = 2.0, -- Enough time for water particles to settle
    active = true
  })
end

-- Create torch fire particles for decorative effect
function particles.createTorchFireParticles(x, y)
  -- Create small fire particle texture
  local fireTexture = love.graphics.newCanvas(2, 2)
  love.graphics.setCanvas(fireTexture)
  love.graphics.setColor(1, 0.8, 0.2, 1) -- Orange fire color
  love.graphics.rectangle("fill", 0, 0, 2, 2)
  love.graphics.setCanvas()

  -- Create particle system for fire effect
  local psystem = love.graphics.newParticleSystem(fireTexture, 15)

  -- Configure fire particles
  psystem:setParticleLifetime(0.3, 0.8)    -- Short lifetime for quick flicker
  psystem:setEmissionRate(25)              -- Continuous emission rate
  psystem:setEmissionArea("uniform", 2, 2) -- Small area around flame

  -- Initial velocity - particles rise upward with slight sway
  psystem:setSpeed(10, 30)           -- Slow to medium speed
  psystem:setDirection(-math.pi / 2) -- Upward direction
  psystem:setSpread(math.pi / 6)     -- 30 degree spread for natural flame movement

  -- No gravity for fire particles (they rise with hot air)
  psystem:setLinearAcceleration(0, -20, 0, -50) -- Slight upward acceleration (hot air)
  psystem:setLinearDamping(2, 4)                -- Air resistance slows them down

  -- Size progression
  psystem:setSizes(0.5, 1.2, 0.1) -- Start small, grow, then shrink
  psystem:setSizeVariation(0.4)   -- Size variation for organic look

  -- Color progression - fire effect
  psystem:setColors(
    1.0, 0.9, 0.2, 0.8, -- Bright yellow-orange at start
    1.0, 0.5, 0.1, 0.6, -- Orange in middle
    0.8, 0.2, 0.1, 0.2, -- Deep red/orange
    0.3, 0.3, 0.3, 0.0  -- Dark smoke at end (transparent)
  )

  -- Position the particle system at torch flame location
  psystem:setPosition(x, y)

  -- Start emitting (this will be a continuous system)
  psystem:emit(8) -- Initial burst

  -- Store particle system
  table.insert(particleSystems, {
    system = psystem,
    active = true,
    timer = 0,
    maxTime = 0.8,        -- Short lifetime for continuous regeneration
    torchParticles = true -- Mark as torch particles for special handling
  })
end

-- Create torch spark particles for extra flair
function particles.createTorchSparks(x, y)
  -- Create spark texture
  local sparkTexture = love.graphics.newCanvas(1, 1)
  love.graphics.setCanvas(sparkTexture)
  love.graphics.setColor(1, 1, 0.5, 1) -- Bright yellow spark
  love.graphics.rectangle("fill", 0, 0, 1, 1)
  love.graphics.setCanvas()

  -- Create particle system for sparks
  local psystem = love.graphics.newParticleSystem(sparkTexture, 5)

  -- Configure spark particles
  psystem:setParticleLifetime(0.2, 0.6)    -- Very short lifetime
  psystem:setEmissionRate(8)               -- Low emission rate for occasional sparks
  psystem:setEmissionArea("uniform", 1, 1) -- Very small area

  -- Sparks have more random movement
  psystem:setSpeed(15, 50)           -- Medium speed
  psystem:setDirection(-math.pi / 2) -- Generally upward
  psystem:setSpread(math.pi / 3)     -- Wider spread than fire

  -- Light gravity affects sparks
  psystem:setLinearAcceleration(0, 50, 0, 150) -- Light downward pull
  psystem:setLinearDamping(3, 6)               -- High air resistance

  -- Size
  psystem:setSizes(1.0, 0.8, 0.1) -- Shrink over time
  psystem:setSizeVariation(0.3)

  -- Bright spark colors
  psystem:setColors(
    1.0, 1.0, 0.7, 1.0, -- Bright white-yellow
    1.0, 0.8, 0.3, 0.8, -- Golden yellow
    1.0, 0.4, 0.1, 0.4, -- Orange
    0.5, 0.1, 0.0, 0.0  -- Dark red (fade out)
  )

  -- Position at torch flame
  psystem:setPosition(x, y)

  -- Emit small burst of sparks
  psystem:emit(2)

  -- Store particle system
  table.insert(particleSystems, {
    system = psystem,
    active = true,
    timer = 0,
    maxTime = 0.8
  })
end

-- Create torch smoke particles for atmospheric effect
function particles.createTorchSmoke(x, y)
  -- Create smoke particle texture
  local smokeTexture = love.graphics.newCanvas(3, 3)
  love.graphics.setCanvas(smokeTexture)
  love.graphics.setColor(0.4, 0.4, 0.4, 1) -- Gray smoke
  love.graphics.rectangle("fill", 0, 0, 3, 3)
  love.graphics.setCanvas()

  -- Create particle system for smoke
  local psystem = love.graphics.newParticleSystem(smokeTexture, 8)

  -- Configure smoke particles
  psystem:setParticleLifetime(1.0, 2.0)    -- Longer lifetime for smoke
  psystem:setEmissionRate(12)              -- Low emission rate
  psystem:setEmissionArea("uniform", 3, 2) -- Small area around flame top

  -- Smoke rises slowly
  psystem:setSpeed(5, 15)            -- Very slow movement
  psystem:setDirection(-math.pi / 2) -- Upward
  psystem:setSpread(math.pi / 8)     -- Narrow spread (smoke rises straight)

  -- No gravity, smoke dissipates upward
  psystem:setLinearAcceleration(0, -10, 0, -5) -- Slight upward drift
  psystem:setLinearDamping(1, 2)               -- Light air resistance

  -- Size progression - smoke particles grow as they rise
  psystem:setSizes(0.3, 1.0, 1.5, 0.1) -- Start small, grow larger, then fade
  psystem:setSizeVariation(0.5)        -- Good size variation

  -- Color progression - gray smoke that becomes transparent
  psystem:setColors(
    0.3, 0.3, 0.3, 0.4, -- Dark gray, semi-transparent
    0.4, 0.4, 0.4, 0.3, -- Medium gray
    0.5, 0.5, 0.5, 0.2, -- Light gray
    0.6, 0.6, 0.6, 0.0  -- Very light gray, fully transparent
  )

  -- Position at top of flame
  psystem:setPosition(x, y)

  -- Emit small amount of smoke
  psystem:emit(3)

  -- Store particle system
  table.insert(particleSystems, {
    system = psystem,
    active = true,
    timer = 0,
    maxTime = 2.5 -- Long enough for smoke to dissipate
  })
end

-- Create torch ember particles for extra realism
function particles.createTorchEmbers(x, y)
  -- Create ember texture
  local emberTexture = love.graphics.newCanvas(2, 2)
  love.graphics.setCanvas(emberTexture)
  love.graphics.setColor(1, 0.3, 0.1, 1) -- Hot ember color
  love.graphics.rectangle("fill", 0, 0, 2, 2)
  love.graphics.setCanvas()

  -- Create particle system for embers
  local psystem = love.graphics.newParticleSystem(emberTexture, 4)

  -- Configure ember particles
  psystem:setParticleLifetime(0.8, 1.5)    -- Medium lifetime
  psystem:setEmissionRate(6)               -- Low emission rate
  psystem:setEmissionArea("uniform", 2, 1) -- Small area

  -- Embers have varied movement
  psystem:setSpeed(8, 25)            -- Slow to medium speed
  psystem:setDirection(-math.pi / 2) -- Generally upward
  psystem:setSpread(math.pi / 4)     -- Medium spread

  -- Gravity pulls embers down eventually
  psystem:setLinearAcceleration(0, 80, 0, 120) -- Light downward pull
  psystem:setLinearDamping(2, 4)               -- Air resistance

  -- Size stays fairly constant
  psystem:setSizes(0.8, 1.0, 0.3) -- Slight shrink as they cool
  psystem:setSizeVariation(0.3)

  -- Ember colors - hot to cool
  psystem:setColors(
    1.0, 0.6, 0.1, 1.0, -- Bright orange-yellow
    1.0, 0.3, 0.1, 0.8, -- Orange-red
    0.8, 0.2, 0.1, 0.4, -- Dark red
    0.3, 0.1, 0.1, 0.0  -- Almost black (fade out)
  )

  -- Position at flame area
  psystem:setPosition(x, y)

  -- Emit small burst of embers
  psystem:emit(2)

  -- Store particle system
  table.insert(particleSystems, {
    system = psystem,
    active = true,
    timer = 0,
    maxTime = 2.0
  })
end

-- Update all particle systems
function particles.update(dt)
  for i = #particleSystems, 1, -1 do
    local ps = particleSystems[i]

    if ps.active then
      ps.system:update(dt)
      ps.timer = ps.timer + dt

      -- Remove particle system if it's old or has no active particles
      if ps.timer >= ps.maxTime or ps.system:getCount() == 0 then
        ps.active = false
        table.remove(particleSystems, i)
      end
    end
  end
end

-- Render all particle systems
function particles.draw()
  -- Reset color to white and set proper blend mode
  love.graphics.setColor(1, 1, 1, 1)
  love.graphics.setBlendMode("alpha")

  for _, ps in ipairs(particleSystems) do
    if ps.active then
      love.graphics.draw(ps.system)
    end
  end

  -- Reset color and blend mode after drawing
  love.graphics.setColor(1, 1, 1, 1)
  love.graphics.setBlendMode("alpha")
end

-- Clear all particles (useful for level transitions)
function particles.clear()
  particleSystems = {}
end

-- Get number of active particle systems (for debugging)
function particles.getActiveCount()
  return #particleSystems
end

return particles
