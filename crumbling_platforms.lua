local collision = require("collision")
local constants = require("constants")

local crumblingPlatforms = {}

-- State constants for crumbling platforms
local PLATFORM_STATE = {
  STABLE = "stable",       -- Normal, stable platform
  SHAKING = "shaking",     -- Player stepped on it, starting to shake
  CRUMBLING = "crumbling", -- Actively falling apart
  DESTROYED = "destroyed"  -- Completely gone
}

-- Timing constants
local SHAKE_DURATION = 0.8   -- How long the platform shakes before crumbling
local CRUMBLE_DURATION = 0.5 -- How long the crumbling animation takes
local RESPAWN_TIME = 3.0     -- How long before platform respawns (optional)

-- Initialize a crumbling platform
function crumblingPlatforms.create(x, y, width, height)
  return {
    x = x,
    y = y,
    width = width,
    height = height,
    originalX = x,
    originalY = y,
    originalWidth = width,
    originalHeight = height,
    state = PLATFORM_STATE.STABLE,
    timer = 0,
    shakeOffset = 0,
    particles = {},
    playerWasOn = false,
    respawnTimer = 0
  }
end

-- Update all crumbling platforms
function crumblingPlatforms.update(gameState, dt)
  for _, platform in ipairs(gameState.crumblingPlatforms) do
    crumblingPlatforms.updatePlatform(platform, dt)
  end
end

-- Update individual platform
function crumblingPlatforms.updatePlatform(platform, dt)
  platform.timer = platform.timer + dt

  if platform.state == PLATFORM_STATE.STABLE then
    -- This will be checked in the collision detection
  elseif platform.state == PLATFORM_STATE.SHAKING then
    -- Create shaking effect
    platform.shakeOffset = math.sin(platform.timer * 20) * (2 + platform.timer * 3)

    -- Transition to crumbling after shake duration
    if platform.timer >= SHAKE_DURATION then
      platform.state = PLATFORM_STATE.CRUMBLING
      platform.timer = 0
      -- Create initial particles
      crumblingPlatforms.createParticles(platform)
    end
  elseif platform.state == PLATFORM_STATE.CRUMBLING then
    -- Platform falls and shrinks
    platform.y = platform.y + dt * 100 -- Fall speed
    platform.width = platform.width - dt * platform.originalWidth / CRUMBLE_DURATION
    platform.height = platform.height - dt * platform.originalHeight / CRUMBLE_DURATION

    -- Keep platform centered while shrinking
    platform.x = platform.originalX + (platform.originalWidth - platform.width) / 2

    -- Update particles
    crumblingPlatforms.updateParticles(platform, dt)

    -- Transition to destroyed
    if platform.timer >= CRUMBLE_DURATION then
      platform.state = PLATFORM_STATE.DESTROYED
      platform.timer = 0
      platform.respawnTimer = 0
    end
  elseif platform.state == PLATFORM_STATE.DESTROYED then
    -- Platform is gone, update respawn timer
    platform.respawnTimer = platform.respawnTimer + dt

    -- Optional: Respawn platform after some time
    if platform.respawnTimer >= RESPAWN_TIME then
      crumblingPlatforms.respawnPlatform(platform)
    end
  end
end

-- Create particle effects when platform starts crumbling
function crumblingPlatforms.createParticles(platform)
  platform.particles = {}

  -- Create several particles from the platform
  for i = 1, constants.CRUMBLE_PARTICLE_COUNT do
    local particle = {
      x = platform.x + math.random() * platform.width,
      y = platform.y + math.random() * platform.height,
      velocityX = (math.random() - 0.5) * constants.CRUMBLE_PARTICLE_VELOCITY_RANGE,
      velocityY = -math.random() *
          (constants.CRUMBLE_PARTICLE_MAX_VELOCITY_Y - constants.CRUMBLE_PARTICLE_MIN_VELOCITY_Y) -
          constants.CRUMBLE_PARTICLE_MIN_VELOCITY_Y,
      size = constants.CRUMBLE_PARTICLE_MIN_SIZE +
          math.random() * (constants.CRUMBLE_PARTICLE_MAX_SIZE - constants.CRUMBLE_PARTICLE_MIN_SIZE),
      life = constants.CRUMBLE_PARTICLE_MIN_LIFE +
          math.random() * (constants.CRUMBLE_PARTICLE_MAX_LIFE - constants.CRUMBLE_PARTICLE_MIN_LIFE),
      maxLife = constants.CRUMBLE_PARTICLE_MIN_LIFE +
          math.random() * (constants.CRUMBLE_PARTICLE_MAX_LIFE - constants.CRUMBLE_PARTICLE_MIN_LIFE)
    }
    particle.maxLife = particle.life
    table.insert(platform.particles, particle)
  end
end

-- Update particle effects
function crumblingPlatforms.updateParticles(platform, dt)
  for i = #platform.particles, 1, -1 do
    local particle = platform.particles[i]

    -- Update position
    particle.x = particle.x + particle.velocityX * dt
    particle.y = particle.y + particle.velocityY * dt

    -- Apply gravity to particles
    particle.velocityY = particle.velocityY + 300 * dt

    -- Update life
    particle.life = particle.life - dt

    -- Remove dead particles
    if particle.life <= 0 then
      table.remove(platform.particles, i)
    end
  end
end

-- Respawn platform to original state
function crumblingPlatforms.respawnPlatform(platform)
  platform.x = platform.originalX
  platform.y = platform.originalY
  platform.width = platform.originalWidth
  platform.height = platform.originalHeight
  platform.state = PLATFORM_STATE.STABLE
  platform.timer = 0
  platform.shakeOffset = 0
  platform.particles = {}
  platform.playerWasOn = false
  platform.respawnTimer = 0
end

-- Check if player steps on platform and trigger crumbling
function crumblingPlatforms.checkPlayerCollision(platform, player)
  if platform.state ~= PLATFORM_STATE.STABLE then
    return false -- Platform is not stable, no collision
  end

  -- Check if player is standing on top of platform
  if collision.checkCollision(player, platform) then
    -- Check if player is coming from above
    local playerBottom = player.y + player.height
    local platformTop = platform.y

    if playerBottom >= platformTop and playerBottom <= platformTop + 10 and player.velocityY >= 0 then
      -- Player is standing on platform - start crumbling process
      if not platform.playerWasOn then
        platform.state = PLATFORM_STATE.SHAKING
        platform.timer = 0
        platform.playerWasOn = true
      end
      return true -- Platform can still support player during shaking
    end
  end

  return false
end

-- Get platform state constants (for external use)
function crumblingPlatforms.getStates()
  return PLATFORM_STATE
end

-- Draw crumbling platforms
function crumblingPlatforms.draw(gameState)
  for _, platform in ipairs(gameState.crumblingPlatforms) do
    if platform.state ~= PLATFORM_STATE.DESTROYED then
      -- Calculate shake effect
      local shakeX = 0
      local shakeY = 0
      if platform.state == PLATFORM_STATE.SHAKING then
        shakeX = platform.shakeOffset * (math.random() - 0.5)
        shakeY = platform.shakeOffset * constants.CRUMBLE_SHAKE_INTENSITY * (math.random() - 0.5)
      end

      -- Calculate opacity based on state
      local alpha = 1.0
      if platform.state == PLATFORM_STATE.CRUMBLING then
        alpha = 1.0 - (platform.timer / 0.5) -- Fade out during crumbling
      end

      -- Main platform body (slightly different color to distinguish from normal platforms)
      love.graphics.setColor(0.8, 0.6, 0.4, alpha) -- Slightly more brown/weathered
      love.graphics.rectangle("fill", platform.x + shakeX, platform.y + shakeY, platform.width, platform.height)

      -- Top surface (lighter color for 3D effect)
      love.graphics.setColor(0.85, 0.65, 0.45, alpha)
      love.graphics.rectangle("fill", platform.x + shakeX, platform.y + shakeY, platform.width, 3)

      -- Side edges for 3D effect (darker)
      love.graphics.setColor(0.5, 0.3, 0.1, alpha)
      love.graphics.rectangle("fill", platform.x + shakeX, platform.y + shakeY + 3, 2, platform.height - 3) -- Left edge
      love.graphics.rectangle("fill", platform.x + platform.width - 2 + shakeX, platform.y + shakeY + 3, 2,
        platform.height - 3)                                                                                -- Right edge

      -- Bottom edge
      love.graphics.rectangle("fill", platform.x + shakeX, platform.y + platform.height - 2 + shakeY, platform.width, 2)

      -- Add cracks texture if shaking or crumbling
      if platform.state == PLATFORM_STATE.SHAKING or platform.state == PLATFORM_STATE.CRUMBLING then
        love.graphics.setColor(0.3, 0.2, 0.1, alpha)
        -- Draw crack lines
        for i = 0, platform.width, 8 do
          if math.random() > 0.7 then -- Random cracks
            love.graphics.rectangle("fill", platform.x + i + shakeX, platform.y + 2 + shakeY, 1, platform.height - 4)
          end
        end

        -- Draw horizontal cracks
        if platform.width > 16 then
          love.graphics.rectangle("fill", platform.x + 4 + shakeX, platform.y + platform.height / 2 + shakeY,
            platform.width - 8, 1)
        end
      else
        -- Normal wood texture for stable platforms
        love.graphics.setColor(0.6, 0.4, 0.2, alpha)
        for i = 0, platform.width - 8, 16 do
          if platform.x + i + 8 < platform.x + platform.width - 1 then
            love.graphics.rectangle("fill", platform.x + i + 8 + shakeX, platform.y + 1 + shakeY, 1, platform.height - 2)
          end
        end
      end

      -- Add warning color effect when shaking
      if platform.state == PLATFORM_STATE.SHAKING then
        local warningAlpha = math.sin(platform.timer * 15) * 0.3 + 0.3
        love.graphics.setColor(1, 0.3, 0.3, warningAlpha * alpha) -- Red warning tint
        love.graphics.rectangle("fill", platform.x + shakeX, platform.y + shakeY, platform.width, platform.height)
      end
    end

    -- Draw particles
    for _, particle in ipairs(platform.particles) do
      local particleAlpha = particle.life / particle.maxLife
      love.graphics.setColor(0.7, 0.5, 0.3, particleAlpha)
      love.graphics.rectangle("fill", particle.x - particle.size / 2, particle.y - particle.size / 2, particle.size,
        particle.size)
    end
  end
end

return crumblingPlatforms
