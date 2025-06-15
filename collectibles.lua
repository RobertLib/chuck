local constants = require("constants")

local collectibles = {}

function collectibles.updateCollectibles(dt, gameState)
  -- Update collectibles animation
  for _, collectible in ipairs(gameState.collectibles) do
    if not collectible.collected then
      collectible.animTime = collectible.animTime + dt * constants.COLLECTIBLE_ANIM_SPEED
    end
  end
end

-- Function to draw animated collectible items (gems/orbs instead of eggs)
function collectibles.drawCollectibleItem(item)
  local x = item.x
  local y = item.y
  local time = item.animTime or 0

  -- Animation effects
  local floatOffset = math.sin(time * 2) * 3     -- Floating up and down
  local pulse = 0.8 + math.sin(time * 4) * 0.2   -- Pulsing size
  local rotation = time * 0.5                    -- Slow rotation effect
  local shimmer = math.sin(time * 6) * 0.3 + 0.7 -- Shimmering brightness

  -- Item center
  local centerX = x + item.width / 2
  local centerY = y + item.height / 2 + floatOffset

  -- Create a crystal/gem-like appearance
  -- Outer glow
  love.graphics.setColor(1, 1, 0.3, 0.3 * shimmer) -- Yellow glow
  love.graphics.circle("fill", centerX, centerY, constants.GLOW_RADIUS * pulse)

  -- Main gem body (diamond shape using rectangles)
  love.graphics.setColor(1, 0.9, 0.2, shimmer) -- Bright yellow/gold

  -- Create diamond shape with rotated squares
  local size = 6 * pulse

  -- Top facet
  love.graphics.setColor(1, 1, 0.8, shimmer) -- Light yellow
  love.graphics.rectangle("fill", centerX - size / 2, centerY - size, size, size / 2)

  -- Middle facet (main body)
  love.graphics.setColor(1, 0.8, 0.2, shimmer) -- Golden yellow
  love.graphics.rectangle("fill", centerX - size, centerY - size / 2, size * 2, size)

  -- Bottom facet
  love.graphics.setColor(0.8, 0.6, 0.1, shimmer) -- Darker gold
  love.graphics.rectangle("fill", centerX - size / 2, centerY + size / 2, size, size / 2)

  -- Inner highlight for sparkle effect
  love.graphics.setColor(1, 1, 1, shimmer * 0.8) -- White highlight
  love.graphics.rectangle("fill", centerX - 2, centerY - 3, 2, 2)
  love.graphics.rectangle("fill", centerX + 1, centerY + 1, 1, 1)

  -- Sparkle particles around the item
  for i = 1, constants.SPARKLE_COUNT do
    local angle = time * 2 + i * (2 * math.pi / constants.SPARKLE_COUNT)
    local sparkleX = centerX + math.cos(angle) * constants.SPARKLE_DISTANCE
    local sparkleY = centerY + math.sin(angle) * 10
    local sparkleAlpha = (math.sin(time * 3 + i) + 1) * 0.3

    love.graphics.setColor(1, 1, 0.8, sparkleAlpha)
    love.graphics.rectangle("fill", sparkleX, sparkleY, 2, 2)
  end
end

return collectibles
