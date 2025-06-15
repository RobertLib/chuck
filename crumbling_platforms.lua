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
local DELAY_BEFORE_SHAKE = 0.5 -- How long player must stay on platform before it starts shaking
local SHAKE_DURATION = 0.25    -- How long the platform shakes before crumbling
local CRUMBLE_DURATION = 0.5   -- How long the crumbling animation takes

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
    standingTimer = 0, -- Timer for how long player is standing on platform
    shakeOffset = 0,
    particles = {},
    playerWasOn = false,
    playerCurrentlyOn = false
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
  -- Ensure all required fields are initialized
  if platform.timer == nil then platform.timer = 0 end
  if platform.standingTimer == nil then platform.standingTimer = 0 end
  if platform.state == nil then platform.state = PLATFORM_STATE.STABLE end
  if platform.shakeOffset == nil then platform.shakeOffset = 0 end
  if platform.particles == nil then platform.particles = {} end
  if platform.playerWasOn == nil then platform.playerWasOn = false end
  if platform.playerCurrentlyOn == nil then platform.playerCurrentlyOn = false end

  platform.timer = platform.timer + dt

  if platform.state == PLATFORM_STATE.STABLE then
    -- Check if player is currently on platform
    if platform.playerCurrentlyOn then
      platform.standingTimer = platform.standingTimer + dt
      -- Start shaking only after delay
      if platform.standingTimer >= DELAY_BEFORE_SHAKE then
        platform.state = PLATFORM_STATE.SHAKING
        platform.timer = 0
      end
    else
      -- Reset standing timer if player is not on platform
      platform.standingTimer = 0
    end

    -- Reset playerCurrentlyOn flag for next frame
    platform.playerCurrentlyOn = false
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
    -- Platform crumbles in place with minimal falling
    -- Slight downward movement instead of fast falling
    platform.y = platform.y + dt * 30 -- Much slower fall speed

    -- Keep original dimensions - no shrinking
    -- platform.width and platform.height stay the same

    -- Update particles
    crumblingPlatforms.updateParticles(platform, dt)

    -- Transition to destroyed
    if platform.timer >= CRUMBLE_DURATION then
      platform.state = PLATFORM_STATE.DESTROYED
      platform.timer = 0
      -- Clear all particles when platform is destroyed
      platform.particles = {}
    end
  elseif platform.state == PLATFORM_STATE.DESTROYED then
    -- Platform stays destroyed permanently - no respawn
    -- Clear particles if any remain
    platform.particles = {}
  end
end

-- Create particle effects when platform starts crumbling
function crumblingPlatforms.createParticles(platform)
  platform.particles = {}

  -- Create more particles for better visual effect
  local particleCount = constants.CRUMBLE_PARTICLE_COUNT or 15

  for i = 1, particleCount do
    local particle = {
      x = platform.x + math.random() * platform.width,
      y = platform.y + math.random() * platform.height,
      velocityX = (math.random() - 0.5) * (constants.CRUMBLE_PARTICLE_VELOCITY_RANGE or 100),
      velocityY = -math.random() * 50 - 20, -- Less dramatic upward velocity
      size = (constants.CRUMBLE_PARTICLE_MIN_SIZE or 2) +
          math.random() * ((constants.CRUMBLE_PARTICLE_MAX_SIZE or 4) - (constants.CRUMBLE_PARTICLE_MIN_SIZE or 2)),
      life = (constants.CRUMBLE_PARTICLE_MIN_LIFE or 1.0) +
          math.random() * ((constants.CRUMBLE_PARTICLE_MAX_LIFE or 2.0) - (constants.CRUMBLE_PARTICLE_MIN_LIFE or 1.0)),
      rotationSpeed = (math.random() - 0.5) * 5 -- Add rotation to particles
    }
    particle.maxLife = particle.life
    table.insert(platform.particles, particle)
  end

  -- Add some bigger chunks that fall more slowly
  for i = 1, 3 do
    local chunk = {
      x = platform.x + math.random() * platform.width,
      y = platform.y + math.random() * platform.height,
      velocityX = (math.random() - 0.5) * 30,
      velocityY = -math.random() * 20 - 5,
      size = 6 + math.random() * 4,
      life = 1.5 + math.random() * 1.0,
      rotationSpeed = (math.random() - 0.5) * 2
    }
    chunk.maxLife = chunk.life
    table.insert(platform.particles, chunk)
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

-- Respawn platform to original state (removed - platforms don't respawn)
-- function crumblingPlatforms.respawnPlatform(platform)
--   platform.x = platform.originalX
--   platform.y = platform.originalY
--   platform.width = platform.originalWidth
--   platform.height = platform.originalHeight
--   platform.state = PLATFORM_STATE.STABLE
--   platform.timer = 0
--   platform.standingTimer = 0
--   platform.shakeOffset = 0
--   platform.particles = {}
--   platform.playerWasOn = false
--   platform.playerCurrentlyOn = false
-- end

-- Check if player steps on platform and track standing time
function crumblingPlatforms.checkPlayerCollision(platform, player)
  if platform.state == PLATFORM_STATE.DESTROYED then
    return false -- Platform is destroyed, no collision
  end

  -- Check if player is standing on top of platform
  if collision.checkCollision(player, platform) then
    -- Check if player is coming from above
    local playerBottom = player.y + player.height
    local platformTop = platform.y

    if playerBottom >= platformTop and playerBottom <= platformTop + 10 and player.velocityY >= 0 then
      -- Player is standing on platform
      platform.playerCurrentlyOn = true

      -- Only allow collision if platform is stable or shaking
      if platform.state == PLATFORM_STATE.STABLE or platform.state == PLATFORM_STATE.SHAKING then
        return true
      end
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
  -- Ensure that crumblingPlatforms is a table
  local platforms = gameState.crumblingPlatforms or {}
  for _, platform in ipairs(platforms) do
    if platform.state ~= PLATFORM_STATE.DESTROYED then
      -- Calculate shake effect
      local shakeX = 0
      local shakeY = 0
      if platform.state == PLATFORM_STATE.SHAKING then
        -- Ensure shakeOffset is initialized
        if not platform.shakeOffset then
          platform.shakeOffset = 0
        end
        shakeX = platform.shakeOffset * (math.random() - 0.5)
        shakeY = platform.shakeOffset * constants.CRUMBLE_SHAKE_INTENSITY * (math.random() - 0.5)
      end

      -- Calculate alpha for different states
      local alpha = 1.0
      local rotationAngle = 0
      local scaleX = 1.0
      local scaleY = 1.0

      if platform.state == PLATFORM_STATE.CRUMBLING then
        -- More realistic crumbling - slight rotation and no shrinking
        rotationAngle = (platform.timer / CRUMBLE_DURATION) * 0.3 * (math.random() - 0.5)
        -- Slight scale variation to simulate breaking apart
        local progress = platform.timer / CRUMBLE_DURATION
        scaleX = 1.0 - progress * 0.1 * math.random()
        scaleY = 1.0 - progress * 0.05 * math.random()
      end

      -- Save current graphics state
      love.graphics.push()

      -- Apply transformations for crumbling effect
      if platform.state == PLATFORM_STATE.CRUMBLING then
        love.graphics.translate(platform.x + platform.width / 2 + shakeX, platform.y + platform.height / 2 + shakeY)
        love.graphics.rotate(rotationAngle)
        love.graphics.scale(scaleX, scaleY)
        love.graphics.translate(-platform.width / 2, -platform.height / 2)
      end

      -- Main platform body - subtly different from normal platforms
      -- Slightly more weathered look with tiny visual hints it might be unstable
      local baseR, baseG, baseB = 0.75, 0.55, 0.35 -- Slightly more muted than normal platforms
      if platform.state == PLATFORM_STATE.STABLE then
        -- Add very subtle aging spots only visible on close inspection
        love.graphics.setColor(baseR, baseG, baseB, alpha)
        love.graphics.rectangle("fill",
          platform.state == PLATFORM_STATE.CRUMBLING and 0 or platform.x + shakeX,
          platform.state == PLATFORM_STATE.CRUMBLING and 0 or platform.y + shakeY,
          platform.width, platform.height)

        -- Subtle darker spots to hint at weakness (very faint)
        love.graphics.setColor(baseR - 0.1, baseG - 0.1, baseB - 0.05, alpha * 0.3)
        for i = 0, platform.width, 12 do
          if math.random() > 0.8 then
            love.graphics.rectangle("fill",
              (platform.state == PLATFORM_STATE.CRUMBLING and 0 or platform.x + shakeX) + i,
              (platform.state == PLATFORM_STATE.CRUMBLING and 0 or platform.y + shakeY) + 1,
              2, 2)
          end
        end
      else
        love.graphics.setColor(baseR, baseG, baseB, alpha)
        love.graphics.rectangle("fill",
          platform.state == PLATFORM_STATE.CRUMBLING and 0 or platform.x + shakeX,
          platform.state == PLATFORM_STATE.CRUMBLING and 0 or platform.y + shakeY,
          platform.width, platform.height)
      end

      -- Top surface (lighter color for 3D effect)
      love.graphics.setColor(baseR + 0.1, baseG + 0.1, baseB + 0.1, alpha)
      love.graphics.rectangle("fill",
        platform.state == PLATFORM_STATE.CRUMBLING and 0 or platform.x + shakeX,
        platform.state == PLATFORM_STATE.CRUMBLING and 0 or platform.y + shakeY,
        platform.width, 3)

      -- Side edges for 3D effect (darker)
      love.graphics.setColor(baseR - 0.25, baseG - 0.25, baseB - 0.25, alpha)
      love.graphics.rectangle("fill",
        platform.state == PLATFORM_STATE.CRUMBLING and 0 or platform.x + shakeX,
        (platform.state == PLATFORM_STATE.CRUMBLING and 0 or platform.y + shakeY) + 3,
        2, platform.height - 3) -- Left edge
      love.graphics.rectangle("fill",
        (platform.state == PLATFORM_STATE.CRUMBLING and 0 or platform.x + shakeX) + platform.width - 2,
        (platform.state == PLATFORM_STATE.CRUMBLING and 0 or platform.y + shakeY) + 3,
        2, platform.height - 3) -- Right edge

      -- Bottom edge
      love.graphics.rectangle("fill",
        platform.state == PLATFORM_STATE.CRUMBLING and 0 or platform.x + shakeX,
        (platform.state == PLATFORM_STATE.CRUMBLING and 0 or platform.y + shakeY) + platform.height - 2,
        platform.width, 2)

      -- Add texture based on state
      if platform.state == PLATFORM_STATE.SHAKING or platform.state == PLATFORM_STATE.CRUMBLING then
        -- Show subtle cracks when shaking or crumbling - much less visible
        love.graphics.setColor(baseR - 0.08, baseG - 0.08, baseB - 0.05, alpha * 0.4)
        -- Vertical crack lines - thinner and less frequent
        for i = 0, platform.width, 12 do
          if math.random() > 0.8 then
            love.graphics.rectangle("fill",
              (platform.state == PLATFORM_STATE.CRUMBLING and 0 or platform.x + shakeX) + i,
              (platform.state == PLATFORM_STATE.CRUMBLING and 0 or platform.y + shakeY) + 3,
              1, platform.height - 6)
          end
        end

        -- Main horizontal crack - much more subtle
        if platform.width > 16 then
          love.graphics.setColor(baseR - 0.06, baseG - 0.06, baseB - 0.04, alpha * 0.3)
          love.graphics.rectangle("fill",
            (platform.state == PLATFORM_STATE.CRUMBLING and 0 or platform.x + shakeX) + 6,
            (platform.state == PLATFORM_STATE.CRUMBLING and 0 or platform.y + shakeY) + platform.height / 2,
            platform.width - 12, 1)
        end
      else
        -- Subtle wood grain texture for stable platforms
        love.graphics.setColor(baseR - 0.1, baseG - 0.1, baseB - 0.05, alpha * 0.6)
        for i = 0, platform.width - 8, 16 do
          if platform.x + i + 8 < platform.x + platform.width - 1 then
            love.graphics.rectangle("fill",
              (platform.state == PLATFORM_STATE.CRUMBLING and 0 or platform.x + shakeX) + i + 8,
              (platform.state == PLATFORM_STATE.CRUMBLING and 0 or platform.y + shakeY) + 1,
              1, platform.height - 2)
          end
        end
      end

      -- Restore graphics state
      love.graphics.pop()
    end

    -- Draw particles (only if platform is crumbling and has particles)
    if platform.state == PLATFORM_STATE.CRUMBLING then
      local particles = platform.particles or {}
      for _, particle in ipairs(particles) do
        local particleAlpha = particle.life / particle.maxLife
        -- More varied particle colors - wood splinters
        local r = 0.6 + math.random() * 0.2
        local g = 0.4 + math.random() * 0.2
        local b = 0.2 + math.random() * 0.1
        love.graphics.setColor(r, g, b, particleAlpha)

        -- Draw particles as small rectangles with slight rotation for more realistic look
        love.graphics.push()
        love.graphics.translate(particle.x, particle.y)
        love.graphics.rotate(particle.x * 0.1) -- Simple rotation based on position
        love.graphics.rectangle("fill", -particle.size / 2, -particle.size / 2, particle.size, particle.size)
        love.graphics.pop()
      end
    end
  end
end

return crumblingPlatforms
