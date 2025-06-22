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
  psystem:setParticleLifetime(0.2, 0.5)      -- Long enough to see the explosion
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

  -- Color and fading - more dramatic
  psystem:setColors(
    1.0, 0.1, 0.1, 1.0, -- Very bright red at start
    0.8, 0.0, 0.0, 0.9, -- Bright red in middle
    0.3, 0.0, 0.0, 0.0  -- Dark and transparent at end
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
    maxTime = 0.8 -- Enough time for massive blood explosion to play out
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
    psystem:setDirection(math.atan2(initialSpeedY, initialSpeedX))
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
  psystem:setSpeed(50, 150)        -- Medium speed for realistic splash
  psystem:setDirection(-math.pi / 2) -- Base direction (upward)
  psystem:setSpread(math.pi / 3)   -- 60 degree spread

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
  love.graphics.setBlendMode("alpha")

  for _, ps in ipairs(particleSystems) do
    if ps.active then
      love.graphics.draw(ps.system)
    end
  end

  -- Reset blend mode
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
