local movingPlatforms = {}

function movingPlatforms.create(x, y, width, height, speed, direction, range)
  return {
    x = x,
    y = y,
    width = width,
    height = height,
    speed = speed or 50,         -- pixels per second
    direction = direction or 1,  -- 1 for right/down, -1 for left/up
    range = range or 100,        -- distance to travel before turning around
    startX = x,                  -- starting position for horizontal movement
    startY = y,                  -- starting position for vertical movement
    movementType = "horizontal", -- "horizontal" or "vertical"
    currentOffset = 0            -- current distance from start position
  }
end

function movingPlatforms.createHorizontal(x, y, width, height, speed, range)
  local platform = movingPlatforms.create(x, y, width, height, speed, 1, range)
  platform.movementType = "horizontal"

  -- Set random starting position within the movement range
  local randomOffset = (math.random() - 0.5) * 2 * range -- Random value between -range and +range
  platform.x = platform.startX + randomOffset
  platform.currentOffset = randomOffset

  -- Set random direction
  platform.direction = math.random() > 0.5 and 1 or -1

  return platform
end

function movingPlatforms.createVertical(x, y, width, height, speed, range)
  local platform = movingPlatforms.create(x, y, width, height, speed, 1, range)
  platform.movementType = "vertical"

  -- Set random starting position within the movement range
  local randomOffset = (math.random() - 0.5) * 2 * range -- Random value between -range and +range
  platform.y = platform.startY + randomOffset
  platform.currentOffset = randomOffset

  -- Set random direction
  platform.direction = math.random() > 0.5 and 1 or -1

  return platform
end

function movingPlatforms.update(platforms, dt)
  for _, platform in ipairs(platforms) do
    -- Calculate movement distance for this frame
    local movement = platform.speed * platform.direction * dt

    if platform.movementType == "horizontal" then
      -- Update horizontal position
      platform.x = platform.x + movement
      platform.currentOffset = platform.x - platform.startX

      -- Check if we've reached the movement limit
      if math.abs(platform.currentOffset) >= platform.range then
        -- Reverse direction and clamp position
        platform.direction = -platform.direction
        if platform.currentOffset > 0 then
          platform.x = platform.startX + platform.range
          platform.currentOffset = platform.range
        else
          platform.x = platform.startX - platform.range
          platform.currentOffset = -platform.range
        end
      end
    else -- vertical movement
      -- Update vertical position
      platform.y = platform.y + movement
      platform.currentOffset = platform.y - platform.startY

      -- Check if we've reached the movement limit
      if math.abs(platform.currentOffset) >= platform.range then
        -- Reverse direction and clamp position
        platform.direction = -platform.direction
        if platform.currentOffset > 0 then
          platform.y = platform.startY + platform.range
          platform.currentOffset = platform.range
        else
          platform.y = platform.startY - platform.range
          platform.currentOffset = -platform.range
        end
      end
    end
  end
end

function movingPlatforms.draw(platforms)
  for _, platform in ipairs(platforms) do
    -- Main platform body (darker than normal platforms)
    love.graphics.setColor(0.5, 0.5, 0.5) -- Dark gray for moving platforms
    love.graphics.rectangle("fill", platform.x, platform.y, platform.width, platform.height)

    -- Top surface (lighter color for 3D effect)
    love.graphics.setColor(0.7, 0.7, 0.7)
    love.graphics.rectangle("fill", platform.x, platform.y, platform.width, 3)

    -- Side edges for 3D effect
    love.graphics.setColor(0.3, 0.3, 0.3)                                                                    -- Darker edge
    love.graphics.rectangle("fill", platform.x, platform.y + 3, 2, platform.height - 3)                      -- Left edge
    love.graphics.rectangle("fill", platform.x + platform.width - 2, platform.y + 3, 2, platform.height - 3) -- Right edge

    -- Bottom edge
    love.graphics.rectangle("fill", platform.x, platform.y + platform.height - 2, platform.width, 2)
  end
end

-- Check collision and move player with platform
function movingPlatforms.checkPlayerCollision(player, platforms, dt)
  local playerOnMovingPlatform = false
  local platformVelocityX = 0
  local platformVelocityY = 0

  for _, platform in ipairs(platforms) do
    -- Check if player is standing on the platform
    local playerBottom = player.y + player.height
    local platformTop = platform.y

    -- Player collision detection
    if player.x < platform.x + platform.width and
        player.x + player.width > platform.x and
        player.y < platform.y + platform.height and
        player.y + player.height > platform.y then
      -- Check if player is standing on top of the platform
      if player.velocityY >= 0 and playerBottom >= platformTop and playerBottom <= platformTop + 10 then
        -- Player is on top of the platform
        player.y = platform.y - player.height
        player.velocityY = 0
        player.onGround = true
        playerOnMovingPlatform = true

        -- Calculate platform velocity for this frame
        if platform.movementType == "horizontal" then
          platformVelocityX = platform.speed * platform.direction
        else
          platformVelocityY = platform.speed * platform.direction
        end

        break
      elseif player.velocityY < 0 and player.y >= platform.y + platform.height then
        -- Player hits platform from below
        player.y = platform.y + platform.height
        player.velocityY = 0
      elseif player.velocityX > 0 and player.x < platform.x then
        -- Player hits platform from left
        player.x = platform.x - player.width
      elseif player.velocityX < 0 and player.x > platform.x then
        -- Player hits platform from right
        player.x = platform.x + platform.width
      end
    end
  end

  -- Move player with platform if standing on it
  if playerOnMovingPlatform then
    player.x = player.x + platformVelocityX * dt
    player.y = player.y + platformVelocityY * dt
  end

  return playerOnMovingPlatform
end

return movingPlatforms
