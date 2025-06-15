local constants = require("constants")
local collision = require("collision")
local particles = require("particles")
local audio = require("audio")

local water = {}

-- Water colors
local WATER_COLORS = {
  water = { 0.1, 0.3, 0.6 },
  waterSurface = { 0.2, 0.5, 0.9 },
  waterDeep = { 0.1, 0.3, 0.6, 0.8 },
  waterLight = { 0.4, 0.7, 1, 0.6 }
}

-- Initialize a water area
function water.init(x, y, width, height)
  return {
    x = x,
    y = y,
    width = width,
    height = height
  }
end

-- Water animation constants
local WAVE_AMPLITUDE = 3       -- How high the waves are
local WAVE_FREQUENCY = 2       -- Speed of wave animation
local WATER_SURFACE_HEIGHT = 8 -- Height of water surface
local BUBBLE_SPAWN_RATE = 0.3  -- How often bubbles spawn

-- Bubbles system
local bubbles = {}
local bubbleTimer = 0

function water.update(dt, gameState)
  -- Update bubble system
  bubbleTimer = bubbleTimer + dt

  -- Spawn new bubbles
  if bubbleTimer >= BUBBLE_SPAWN_RATE then
    water.spawnBubble(gameState)
    bubbleTimer = 0
  end

  -- Update existing bubbles
  for i = #bubbles, 1, -1 do
    local bubble = bubbles[i]
    bubble.y = bubble.y - bubble.speed * dt
    bubble.x = bubble.x + math.sin(bubble.y * 0.01) * 10 * dt -- Bubbles move sideways
    bubble.life = bubble.life - dt

    -- Remove bubbles that are too old or above water surface
    if bubble.life <= 0 or bubble.y < bubble.waterSurfaceY - 20 then
      table.remove(bubbles, i)
    end
  end
end

function water.spawnBubble(gameState)
  -- Spawn bubbles from all water areas
  for _, waterArea in ipairs(gameState.water or {}) do
    if love.math.random() < constants.BUBBLE_SPAWN_CHANCE then
      local bubble = {
        x = waterArea.x + love.math.random() * waterArea.width,
        y = waterArea.y + waterArea.height - 10,
        waterSurfaceY = waterArea.y,
        speed = constants.BUBBLE_MIN_SPEED +
            love.math.random() * (constants.BUBBLE_MAX_SPEED - constants.BUBBLE_MIN_SPEED),
        size = constants.BUBBLE_MIN_SIZE + love.math.random() * (constants.BUBBLE_MAX_SIZE - constants.BUBBLE_MIN_SIZE),
        life = constants.BUBBLE_MIN_LIFE + love.math.random() * (constants.BUBBLE_MAX_LIFE - constants.BUBBLE_MIN_LIFE),
        alpha = constants.BUBBLE_MIN_ALPHA +
            love.math.random() * (constants.BUBBLE_MAX_ALPHA - constants.BUBBLE_MIN_ALPHA)
      }
      table.insert(bubbles, bubble)
    end
  end
end

function water.draw(gameState, globalTime)
  if not gameState.water then return end

  for _, waterArea in ipairs(gameState.water) do
    -- Main water body
    love.graphics.setColor(WATER_COLORS.waterDeep) -- Dark blue water
    love.graphics.rectangle("fill", waterArea.x, waterArea.y + WATER_SURFACE_HEIGHT,
      waterArea.width, waterArea.height - WATER_SURFACE_HEIGHT)

    -- Animated water surface
    water.drawWaterSurface(waterArea, globalTime)

    -- Water reflections/shimmer effect
    water.drawWaterReflections(waterArea, globalTime)
  end

  -- Draw bubbles
  water.drawBubbles()
end

function water.drawWaterSurface(waterArea, globalTime)
  local points = {}
  local segments = math.ceil(waterArea.width / 4) -- Finer segments for smoother waves

  for i = 0, segments do
    local x = waterArea.x + (i / segments) * waterArea.width
    local waveOffset = math.sin((x * 0.02) + (globalTime * WAVE_FREQUENCY)) * WAVE_AMPLITUDE
    local y = waterArea.y + waveOffset

    table.insert(points, x)
    table.insert(points, y)
  end

  -- Add bottom points to close the polygon
  table.insert(points, waterArea.x + waterArea.width)
  table.insert(points, waterArea.y + WATER_SURFACE_HEIGHT)
  table.insert(points, waterArea.x)
  table.insert(points, waterArea.y + WATER_SURFACE_HEIGHT)

  -- Draw surface with lighter blue color
  love.graphics.setColor(WATER_COLORS.waterSurface)
  if #points >= 6 then -- Need at least 3 points (6 coordinates)
    love.graphics.polygon("fill", points)
  end

  -- Draw surface highlight
  love.graphics.setColor(WATER_COLORS.waterLight)
  love.graphics.setLineWidth(2)
  if #points >= 4 then
    -- Draw just the wavy top line
    local surfacePoints = {}
    for i = 1, segments * 2, 2 do
      if points[i] and points[i + 1] then
        table.insert(surfacePoints, points[i])
        table.insert(surfacePoints, points[i + 1])
      end
    end
    if #surfacePoints >= 4 then
      love.graphics.line(surfacePoints)
    end
  end
  love.graphics.setLineWidth(1)
end

function water.drawWaterReflections(waterArea, globalTime)
  -- Add subtle shimmer lines in the water
  for i = 1, 3 do
    local shimmerY = waterArea.y + WATER_SURFACE_HEIGHT + (i * 15)
    if shimmerY < waterArea.y + waterArea.height then
      local shimmerOffset = math.sin((globalTime * 3) + i) * 20
      love.graphics.setColor(0.3, 0.6, 1, 0.3 - i * 0.1)
      love.graphics.line(waterArea.x + shimmerOffset, shimmerY,
        waterArea.x + waterArea.width + shimmerOffset, shimmerY)
    end
  end
end

function water.drawBubbles()
  for _, bubble in ipairs(bubbles) do
    love.graphics.setColor(0.8, 0.9, 1, bubble.alpha * (bubble.life / 5))
    love.graphics.circle("fill", bubble.x, bubble.y, bubble.size)

    -- Bubble highlight
    love.graphics.setColor(1, 1, 1, bubble.alpha * 0.5 * (bubble.life / 5))
    love.graphics.circle("fill", bubble.x - bubble.size * 0.3, bubble.y - bubble.size * 0.3, bubble.size * 0.4)
  end
end

function water.checkCollision(gameState)
  if not gameState.water then return false end

  local player = gameState.player
  local playerRect = {
    x = player.x,
    y = player.y,
    width = player.width,
    height = player.height
  }

  for _, waterArea in ipairs(gameState.water) do
    local waterRect = {
      x = waterArea.x,
      y = waterArea.y,
      width = waterArea.width,
      height = waterArea.height
    }

    if collision.checkCollision(playerRect, waterRect) then
      -- Player touched water - create splash effect and return true for death
      particles.createWaterSplash(player.x + player.width / 2, waterArea.y)
      audio.playWaterSplash() -- Play water splash sound
      return true
    end
  end

  return false
end

-- Add water current effect to player when in water
function water.applyWaterPhysics(player, dt, gameState)
  if not gameState.water then return end

  local playerRect = {
    x = player.x,
    y = player.y,
    width = player.width,
    height = player.height
  }

  for _, waterArea in ipairs(gameState.water) do
    local waterRect = {
      x = waterArea.x,
      y = waterArea.y,
      width = waterArea.width,
      height = waterArea.height
    }

    if collision.checkCollision(playerRect, waterRect) then
      -- Apply water current (optional - you can comment this out if you don't want current)
      -- player.x = player.x + CURRENT_STRENGTH * dt

      -- Make player sink faster in water
      player.velocityY = player.velocityY + constants.GRAVITY * dt * 0.5
      break
    end
  end
end

return water
