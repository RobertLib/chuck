local collision = require("collision")
local audio = require("audio")

local crushers = {}

-- Initialize a crusher
function crushers.init(x, y, width, height, direction, length)
  return {
    x = x,
    y = y,
    width = width or 40,
    height = height or 20,
    direction = direction or "down", -- only "up" or "down" allowed
    length = length or 60,
    state = "idle",
    timer = 0,
    currentLength = 0,
    maxLength = length or 60,
    activated = false
  }
end

-- Crusher states
local CRUSHER_IDLE = "idle"
local CRUSHER_EXTENDING = "extending"
local CRUSHER_RETRACTING = "retracting"
local CRUSHER_COOLDOWN = "cooldown"

-- Crusher settings
local CRUSHER_EXTEND_SPEED = 150       -- pixels per second
local CRUSHER_RETRACT_SPEED = 200      -- pixels per second
local CRUSHER_COOLDOWN_TIME = 2.0      -- seconds before crusher can activate again
local CRUSHER_DETECTION_DISTANCE = 100 -- how far to check for player

function crushers.update(gameState, dt)
  local player = gameState.player

  for _, crusher in ipairs(gameState.crushers) do
    -- Initialize crusher state if not set
    if not crusher.maxLength then
      crusher.maxLength = crusher.length or 60
    end

    -- Update timer
    crusher.timer = crusher.timer + dt

    local playerCenterX = player.x + player.width / 2
    local playerCenterY = player.y + player.height / 2

    -- Check if player is in detection range and there's clear path
    local inRange = false
    local clearPath = true

    if crusher.direction == "down" then
      -- Check if player is below crusher and within horizontal range
      if playerCenterX >= crusher.x and playerCenterX <= crusher.x + crusher.width and
          playerCenterY >= crusher.y + crusher.height and
          playerCenterY <= crusher.y + crusher.height + CRUSHER_DETECTION_DISTANCE then
        inRange = true

        -- Check for obstacles between crusher and player
        local checkY = crusher.y + crusher.height
        while checkY < playerCenterY do
          -- Check for platforms that would block the crusher
          for _, platform in ipairs(gameState.platforms) do
            if checkY >= platform.y and checkY <= platform.y + platform.height and
                crusher.x + crusher.width > platform.x and crusher.x < platform.x + platform.width then
              clearPath = false
              break
            end
          end
          if not clearPath then break end
          checkY = checkY + 10
        end
      end
    elseif crusher.direction == "up" then
      -- Check if player is above crusher
      if playerCenterX >= crusher.x and playerCenterX <= crusher.x + crusher.width and
          playerCenterY <= crusher.y and
          playerCenterY >= crusher.y - CRUSHER_DETECTION_DISTANCE then
        inRange = true

        -- Check for obstacles between crusher and player
        local checkY = crusher.y
        while checkY > playerCenterY do
          for _, platform in ipairs(gameState.platforms) do
            if checkY >= platform.y and checkY <= platform.y + platform.height and
                crusher.x + crusher.width > platform.x and crusher.x < platform.x + platform.width then
              clearPath = false
              break
            end
          end
          if not clearPath then break end
          checkY = checkY - 10
        end
      end
    end

    -- State machine for crusher behavior
    if crusher.state == CRUSHER_IDLE then
      if inRange and clearPath and crusher.timer >= CRUSHER_COOLDOWN_TIME then
        crusher.state = CRUSHER_EXTENDING
        crusher.activated = true
        audio.playCrusher() -- Play crusher sound when starting to extend
      end
    elseif crusher.state == CRUSHER_EXTENDING then
      crusher.currentLength = crusher.currentLength + CRUSHER_EXTEND_SPEED * dt
      if crusher.currentLength >= crusher.maxLength then
        crusher.currentLength = crusher.maxLength
        crusher.state = CRUSHER_RETRACTING
      end
    elseif crusher.state == CRUSHER_RETRACTING then
      crusher.currentLength = crusher.currentLength - CRUSHER_RETRACT_SPEED * dt
      if crusher.currentLength <= 0 then
        crusher.currentLength = 0
        crusher.state = CRUSHER_COOLDOWN
        crusher.timer = 0
        crusher.activated = false
      end
    elseif crusher.state == CRUSHER_COOLDOWN then
      if crusher.timer >= CRUSHER_COOLDOWN_TIME then
        crusher.state = CRUSHER_IDLE
        crusher.timer = 0
      end
    end
  end
end

-- Check if player is being crushed and should take damage
function crushers.checkPlayerCollision(gameState)
  local player = gameState.player

  for _, crusher in ipairs(gameState.crushers) do
    if crusher.state == CRUSHER_EXTENDING and crusher.currentLength > 0 then
      -- Create collision box for the extended part of the crusher
      local crushBox = {
        x = crusher.x,
        y = crusher.y,
        width = crusher.width,
        height = crusher.height
      }

      if crusher.direction == "down" then
        crushBox.y = crusher.y + crusher.height
        crushBox.height = crusher.currentLength
      elseif crusher.direction == "up" then
        crushBox.y = crusher.y - crusher.currentLength
        crushBox.height = crusher.currentLength
      end

      -- Check collision with player
      if collision.checkCollision(player, crushBox) then
        return true
      end
    end
  end

  return false
end

-- Draw crushers (mechanical pressing traps)
function crushers.draw(gameState)
  for _, crusher in ipairs(gameState.crushers) do
    local x = crusher.x
    local y = crusher.y
    local width = crusher.width
    local height = crusher.height
    local direction = crusher.direction or "down"
    local currentLength = crusher.currentLength or 0

    -- Main crusher body (metallic dark gray)
    love.graphics.setColor(0.3, 0.3, 0.3)
    love.graphics.rectangle("fill", x, y, width, height)

    -- Crusher body details (lighter edges for 3D effect)
    love.graphics.setColor(0.5, 0.5, 0.5)
    love.graphics.rectangle("fill", x, y, width, 2)  -- Top edge
    love.graphics.rectangle("fill", x, y, 2, height) -- Left edge

    -- Darker bottom and right edges
    love.graphics.setColor(0.2, 0.2, 0.2)
    love.graphics.rectangle("fill", x, y + height - 2, width, 2) -- Bottom edge
    love.graphics.rectangle("fill", x + width - 2, y, 2, height) -- Right edge

    -- Draw mechanical details on crusher body
    love.graphics.setColor(0.4, 0.4, 0.4)
    -- Rivets or bolts
    for i = 4, width - 4, 8 do
      for j = 4, height - 4, 8 do
        love.graphics.rectangle("fill", x + i, y + j, 2, 2)
      end
    end

    -- Draw extended crushing part if active
    if currentLength > 0 then
      local crushX, crushY, crushWidth, crushHeight = x, y, width, height

      if direction == "down" then
        crushY = y + height
        crushHeight = currentLength
      elseif direction == "up" then
        crushY = y - currentLength
        crushHeight = currentLength
      end

      -- Extended crusher part (slightly darker)
      love.graphics.setColor(0.25, 0.25, 0.25)
      love.graphics.rectangle("fill", crushX, crushY, crushWidth, crushHeight)

      -- Dangerous crushing surface (red-tinted)
      love.graphics.setColor(0.4, 0.2, 0.2)
      if direction == "down" then
        love.graphics.rectangle("fill", crushX, crushY + crushHeight - 4, crushWidth, 4)
      elseif direction == "up" then
        love.graphics.rectangle("fill", crushX, crushY, crushWidth, 4)
      end

      -- Warning stripes on extended part
      love.graphics.setColor(0.8, 0.8, 0.1)
      for i = 0, crushWidth, 12 do
        if i + 6 < crushWidth then
          love.graphics.rectangle("fill", crushX + i + 2, crushY + 2, 2, crushHeight - 4)
        end
      end
    end

    -- Draw direction indicator on crusher body
    love.graphics.setColor(0.8, 0.2, 0.2)
    local arrowSize = math.min(width, height) / 4
    local centerX = x + width / 2
    local centerY = y + height / 2

    if direction == "down" then
      love.graphics.polygon("fill",
        centerX, centerY + arrowSize,
        centerX - arrowSize / 2, centerY - arrowSize / 2,
        centerX + arrowSize / 2, centerY - arrowSize / 2)
    elseif direction == "up" then
      love.graphics.polygon("fill",
        centerX, centerY - arrowSize,
        centerX - arrowSize / 2, centerY + arrowSize / 2,
        centerX + arrowSize / 2, centerY + arrowSize / 2)
    end
  end
end

return crushers
