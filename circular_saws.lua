local constants = require("constants")
local collision = require("collision")
local playerDeath = require("player_death")

local circularSaws = {}

-- Initialize a circular saw
function circularSaws.init(x, y, width, height)
  return {
    x = x,
    y = y,
    width = width or constants.CIRCULAR_SAW_WIDTH,
    height = height or constants.CIRCULAR_SAW_HEIGHT,
    rotation = 0,
    isRotating = true,
    timer = 0,
    rotateTime = math.random() * (constants.CIRCULAR_SAW_ROTATE_TIME_MAX - constants.CIRCULAR_SAW_ROTATE_TIME_MIN) +
        constants.CIRCULAR_SAW_ROTATE_TIME_MIN,
    pauseTime = math.random() * (constants.CIRCULAR_SAW_PAUSE_TIME_MAX - constants.CIRCULAR_SAW_PAUSE_TIME_MIN) +
        constants.CIRCULAR_SAW_PAUSE_TIME_MIN
  }
end

-- Update circular saws
function circularSaws.update(gameState, dt)
  for _, saw in ipairs(gameState.circularSaws or {}) do
    saw.timer = saw.timer + dt

    if saw.isRotating then
      -- Rotate the saw
      saw.rotation = saw.rotation + constants.CIRCULAR_SAW_ROTATION_SPEED * dt

      -- Check if rotation time is up
      if saw.timer >= saw.rotateTime then
        saw.isRotating = false
        saw.timer = 0
        -- Set new random pause time
        saw.pauseTime = math.random() * (constants.CIRCULAR_SAW_PAUSE_TIME_MAX - constants.CIRCULAR_SAW_PAUSE_TIME_MIN) +
            constants.CIRCULAR_SAW_PAUSE_TIME_MIN
      end
    else
      -- Paused, check if pause time is up
      if saw.timer >= saw.pauseTime then
        saw.isRotating = true
        saw.timer = 0
        -- Set new random rotate time
        saw.rotateTime = math.random() *
            (constants.CIRCULAR_SAW_ROTATE_TIME_MAX - constants.CIRCULAR_SAW_ROTATE_TIME_MIN) +
            constants.CIRCULAR_SAW_ROTATE_TIME_MIN
      end
    end
  end
end

-- Draw circular saws
function circularSaws.draw(gameState)
  for _, saw in ipairs(gameState.circularSaws or {}) do
    local centerX = saw.x + saw.width / 2
    local centerY = saw.y + saw.height / 2
    local radius = saw.width / 2 - 2

    love.graphics.push()
    love.graphics.translate(centerX, centerY)
    love.graphics.rotate(saw.rotation)

    -- Draw the main circular blade (dark metallic gray)
    love.graphics.setColor(0.3, 0.3, 0.3, 1.0)
    love.graphics.circle("fill", 0, 0, radius)

    -- Draw outer rim (lighter gray)
    love.graphics.setColor(0.6, 0.6, 0.6, 1.0)
    love.graphics.setLineWidth(2)
    love.graphics.circle("line", 0, 0, radius)

    -- Draw teeth around the edge
    love.graphics.setColor(0.8, 0.8, 0.8, 1.0)
    local numTeeth = 16
    for i = 1, numTeeth do
      local angle = (i / numTeeth) * 2 * math.pi
      local toothLength = 4
      local innerX = math.cos(angle) * (radius - toothLength)
      local innerY = math.sin(angle) * (radius - toothLength)
      local outerX = math.cos(angle) * radius
      local outerY = math.sin(angle) * radius

      love.graphics.setLineWidth(2)
      love.graphics.line(innerX, innerY, outerX, outerY)
    end

    -- Draw center hub
    love.graphics.setColor(0.2, 0.2, 0.2, 1.0)
    love.graphics.circle("fill", 0, 0, 4)

    -- Draw center highlight
    love.graphics.setColor(0.9, 0.9, 0.9, 1.0)
    love.graphics.circle("fill", 0, 0, 2)

    love.graphics.pop()

    -- Reset line width
    love.graphics.setLineWidth(1)
  end
end

-- Check collision with player
function circularSaws.checkCollision(gameState)
  local player = gameState.player

  for _, saw in ipairs(gameState.circularSaws or {}) do
    -- Only check collision when the saw is rotating
    if saw.isRotating and collision.checkCircleRectCollision(player, saw) then
      if playerDeath.killPlayerAtCenter(gameState, "circularSaw") then
        return true -- Only lose one life per frame
      end
    end
  end
  return false
end

return circularSaws
