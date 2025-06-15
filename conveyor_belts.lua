local conveyorBelts = {}

function conveyorBelts.create(x, y, width, height, speed, direction)
  return {
    x = x,
    y = y,
    width = width,
    height = height,
    speed = speed or 50,        -- belt speed in pixels per second
    direction = direction or 1, -- 1 for right, -1 for left
    animationOffset = 0         -- for belt movement animation
  }
end

function conveyorBelts.update(belts, dt)
  for _, belt in ipairs(belts) do
    -- Update belt animation
    belt.animationOffset = belt.animationOffset + (belt.speed * belt.direction * dt)
    -- Reset offset to prevent overflow
    if math.abs(belt.animationOffset) > belt.width then
      belt.animationOffset = belt.animationOffset % belt.width
    end
  end
end

function conveyorBelts.draw(belts)
  for _, belt in ipairs(belts) do
    -- Base belt color (dark gray)
    love.graphics.setColor(0.3, 0.3, 0.3)
    love.graphics.rectangle("fill", belt.x, belt.y, belt.width, belt.height)

    -- Top surface (lighter for 3D effect)
    love.graphics.setColor(0.4, 0.4, 0.4)
    love.graphics.rectangle("fill", belt.x, belt.y, belt.width, 3)

    -- Edges for 3D effect
    love.graphics.setColor(0.2, 0.2, 0.2)
    love.graphics.rectangle("fill", belt.x, belt.y + 3, 2, belt.height - 3)                  -- Left edge
    love.graphics.rectangle("fill", belt.x + belt.width - 2, belt.y + 3, 2, belt.height - 3) -- Right edge
    love.graphics.rectangle("fill", belt.x, belt.y + belt.height - 2, belt.width, 2)         -- Bottom edge

    -- Animated lines showing movement direction
    love.graphics.setColor(0.6, 0.6, 0.6)
    local lineSpacing = 16
    local lineOffset = belt.animationOffset % lineSpacing

    for i = -lineSpacing, belt.width + lineSpacing, lineSpacing do
      local lineX = belt.x + i + lineOffset
      if lineX >= belt.x and lineX <= belt.x + belt.width - 2 then
        -- Arrows showing direction
        if belt.direction > 0 then
          -- Right arrows
          love.graphics.polygon("fill",
            lineX, belt.y + 2,
            lineX + 4, belt.y + 5,
            lineX, belt.y + 8
          )
        else
          -- Left arrows
          love.graphics.polygon("fill",
            lineX + 4, belt.y + 2,
            lineX, belt.y + 5,
            lineX + 4, belt.y + 8
          )
        end
      end
    end
  end
end

-- Check if player is on conveyor belt
function conveyorBelts.isPlayerOnBelt(player, belt)
  -- Check if player is standing on top of the belt
  local tolerance = 3
  return player.x + player.width > belt.x and
      player.x < belt.x + belt.width and
      player.y + player.height >= belt.y - tolerance and
      player.y + player.height <= belt.y + tolerance and
      player.onGround
end

-- Apply conveyor belt effect to player
function conveyorBelts.applyBeltEffect(belts, player, dt)
  for _, belt in ipairs(belts) do
    if conveyorBelts.isPlayerOnBelt(player, belt) then
      -- Add belt speed to player position
      local beltForce = belt.speed * belt.direction * dt
      player.x = player.x + beltForce

      -- Clamp player position to screen bounds
      if player.x < 0 then
        player.x = 0
      elseif player.x + player.width > love.graphics.getWidth() then
        player.x = love.graphics.getWidth() - player.width
      end

      -- Only one belt can affect the player at a time
      break
    end
  end
end

return conveyorBelts
