local crumbling_platforms = {}

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
function crumbling_platforms.create(x, y, width, height)
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
function crumbling_platforms.update(gameState, dt)
  for _, platform in ipairs(gameState.crumbling_platforms) do
    crumbling_platforms.updatePlatform(platform, dt)
  end
end

-- Update individual platform
function crumbling_platforms.updatePlatform(platform, dt)
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
      crumbling_platforms.createParticles(platform)
    end
  elseif platform.state == PLATFORM_STATE.CRUMBLING then
    -- Platform falls and shrinks
    platform.y = platform.y + dt * 100 -- Fall speed
    platform.width = platform.width - dt * platform.originalWidth / CRUMBLE_DURATION
    platform.height = platform.height - dt * platform.originalHeight / CRUMBLE_DURATION

    -- Keep platform centered while shrinking
    platform.x = platform.originalX + (platform.originalWidth - platform.width) / 2

    -- Update particles
    crumbling_platforms.updateParticles(platform, dt)

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
      crumbling_platforms.respawnPlatform(platform)
    end
  end
end

-- Create particle effects when platform starts crumbling
function crumbling_platforms.createParticles(platform)
  platform.particles = {}

  -- Create several particles from the platform
  for i = 1, 8 do
    local particle = {
      x = platform.x + math.random() * platform.width,
      y = platform.y + math.random() * platform.height,
      velocityX = (math.random() - 0.5) * 100,
      velocityY = -math.random() * 50 - 20,
      size = math.random() * 4 + 2,
      life = math.random() * 1 + 0.5,
      maxLife = math.random() * 1 + 0.5
    }
    particle.maxLife = particle.life
    table.insert(platform.particles, particle)
  end
end

-- Update particle effects
function crumbling_platforms.updateParticles(platform, dt)
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
function crumbling_platforms.respawnPlatform(platform)
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
function crumbling_platforms.checkPlayerCollision(platform, player)
  if platform.state ~= PLATFORM_STATE.STABLE then
    return false -- Platform is not stable, no collision
  end

  -- Check if player is standing on top of platform
  local collision = require("collision")
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
function crumbling_platforms.getStates()
  return PLATFORM_STATE
end

return crumbling_platforms
