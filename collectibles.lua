local constants = require("constants")

local collectibles = {}

function collectibles.updateCollectibles(dt, gameState)
  -- Update collectibles animation
  for _, collectible in ipairs(gameState.collectibles) do
    if not collectible.collected then
      collectible.animTime = collectible.animTime + dt * constants.COLLECTIBLE_ANIM_SPEED
    elseif collectible.isPickingUp then
      -- Update pickup animation
      collectible.pickupTime = collectible.pickupTime + dt
      if collectible.pickupTime >= constants.PICKUP_ANIMATION_DURATION then
        collectible.isPickingUp = false
      end
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

-- Function to draw pickup animation for collected item
function collectibles.drawPickupAnimation(item)
  if not item.isPickingUp then return end

  local x = item.x
  local y = item.y
  local progress = item.pickupTime / constants.PICKUP_ANIMATION_DURATION

  -- Animation values
  local scale = 1 + (constants.PICKUP_SCALE_MAX - 1) * progress -- Grow to max scale
  local alpha = 1 - progress * constants.PICKUP_FADE_SPEED      -- Fade out
  local yOffset = -progress * 50                                -- Float upwards (zvětšeno z 30 na 50)

  -- Clamp alpha to be non-negative
  alpha = math.max(0, alpha)

  -- Item center
  local centerX = x + item.width / 2
  local centerY = y + item.height / 2 + yOffset

  -- Sparkling burst effect
  local sparkles = math.floor(12 * (1 - progress))
  for i = 1, sparkles do
    local angle = i * (2 * math.pi / 12) + item.pickupTime * 4
    local distance = 35 * progress
    local sparkleX = centerX + math.cos(angle) * distance
    local sparkleY = centerY + math.sin(angle) * distance
    local sparkleAlpha = alpha * 0.8

    love.graphics.setColor(1, 1, 1, sparkleAlpha)
    love.graphics.circle("fill", sparkleX, sparkleY, 3)
    love.graphics.setColor(1, 1, 0.5, sparkleAlpha * 0.5)
    love.graphics.circle("fill", sparkleX, sparkleY, 6)
  end

  -- Main item (scaled and fading)
  local size = 6 * scale

  -- Outer glow with scale
  love.graphics.setColor(1, 1, 0.3, 0.4 * alpha)
  love.graphics.circle("fill", centerX, centerY, constants.GLOW_RADIUS * scale)

  -- Main gem body with animation
  love.graphics.setColor(1, 0.9, 0.2, alpha)

  -- Top facet
  love.graphics.setColor(1, 1, 0.8, alpha)
  love.graphics.rectangle("fill", centerX - size / 2, centerY - size, size, size / 2)

  -- Middle facet (main body)
  love.graphics.setColor(1, 0.8, 0.2, alpha)
  love.graphics.rectangle("fill", centerX - size, centerY - size / 2, size * 2, size)

  -- Bottom facet
  love.graphics.setColor(0.8, 0.6, 0.1, alpha)
  love.graphics.rectangle("fill", centerX - size / 2, centerY + size / 2, size, size / 2)

  -- Inner highlight for sparkle effect
  love.graphics.setColor(1, 1, 1, alpha * 0.8)
  love.graphics.rectangle("fill", centerX - 2 * scale, centerY - 3 * scale, 2 * scale, 2 * scale)
  love.graphics.rectangle("fill", centerX + 1 * scale, centerY + 1 * scale, 1 * scale, 1 * scale)
end

return collectibles
