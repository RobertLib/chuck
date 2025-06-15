local constants = require("constants")
local collision = require("collision")
local playerDeath = require("player_death")
local audio = require("audio")

local bats = {}

-- Spawn a new bat from random side
function bats.spawnBat(gameState)
  local bat = {}

  -- Random height between min and max
  bat.y = math.random(constants.BAT_SPAWN_HEIGHT_MIN, constants.BAT_SPAWN_HEIGHT_MAX)
  bat.width = constants.BAT_WIDTH
  bat.height = constants.BAT_HEIGHT

  -- Random direction (left or right) with varied speed
  local speed = constants.BAT_SPEED + math.random() * constants.BAT_SPEED_VARIATION * 2 - constants.BAT_SPEED_VARIATION
  if math.random() < constants.BAT_SPAWN_CHANCE then
    -- Spawn from left, move right
    bat.x = -bat.width
    bat.velocityX = speed
    bat.direction = 1
  else
    -- Spawn from right, move left
    bat.x = constants.SCREEN_WIDTH
    bat.velocityX = -speed
    bat.direction = -1
  end

  bat.velocityY = 0
  bat.animTime = math.random() * 2 * math.pi -- Random start phase for wing animation
  bat.bobPhase = math.random() * 2 * math.pi -- Random start phase for vertical bobbing

  table.insert(gameState.bats, bat)
end

-- Update all bats
function bats.updateBats(dt, gameState)
  -- Update spawn timer
  gameState.batSpawnTimer = gameState.batSpawnTimer + dt

  -- Spawn new bat if timer reached interval (with some randomness)
  local spawnInterval = constants.BAT_SPAWN_INTERVAL + math.random() * 2 - 1 -- ±1 second variation
  if gameState.batSpawnTimer >= spawnInterval then
    bats.spawnBat(gameState)
    gameState.batSpawnTimer = 0
  end

  -- Update existing bats
  for i = #gameState.bats, 1, -1 do
    local bat = gameState.bats[i]

    -- Update animation time
    bat.animTime = bat.animTime + dt * 8 -- Wing flapping speed
    bat.bobPhase = bat.bobPhase + dt * 3 -- Vertical bobbing speed

    -- Move horizontally
    bat.x = bat.x + bat.velocityX * dt

    -- Add slight vertical bobbing motion
    bat.velocityY = math.sin(bat.bobPhase) * 20

    -- Remove bats that have gone off screen
    if (bat.direction > 0 and bat.x > constants.SCREEN_WIDTH) or
        (bat.direction < 0 and bat.x < -bat.width) then
      table.remove(gameState.bats, i)
    end
  end
end

-- Draw a single bat
function bats.drawBat(bat)
  local x = bat.x
  local y = bat.y + math.sin(bat.bobPhase) * 3 -- Vertical bobbing

  -- Wing animation - calculate wing position
  local wingOffset = math.sin(bat.animTime) * 3

  -- Bat colors (more visible, still spooky)
  local bodyColor = { 0.4, 0.2, 0.2 }   -- Lighter brown body
  local wingColor = { 0.3, 0.15, 0.15 } -- Lighter wings
  local eyeColor = { 1.0, 0.3, 0.3 }    -- Brighter red glowing eyes

  -- Determine facing direction for sprite flipping
  local scaleX = bat.direction

  love.graphics.push()
  love.graphics.translate(x + bat.width / 2, y + bat.height / 2)
  love.graphics.scale(scaleX, 1)

  -- Draw subtle outline for better visibility
  love.graphics.setColor(0.7, 0.7, 0.7, 0.7) -- More subtle, semi-transparent outline
  love.graphics.setLineWidth(0.5)

  -- Outline wings
  love.graphics.polygon("line",
    -8, -2 + wingOffset,
    -12, -5 + wingOffset,
    -10, 2 + wingOffset,
    -6, 0 + wingOffset
  )
  love.graphics.polygon("line",
    8, -2 + wingOffset,
    12, -5 + wingOffset,
    10, 2 + wingOffset,
    6, 0 + wingOffset
  )

  -- Outline body and head
  love.graphics.ellipse("line", 0, 0, 4, 6)
  love.graphics.ellipse("line", 0, -4, 3, 3)

  -- Wings (animated)
  love.graphics.setColor(wingColor)
  -- Left wing
  love.graphics.polygon("fill",
    -8, -2 + wingOffset,
    -12, -5 + wingOffset,
    -10, 2 + wingOffset,
    -6, 0 + wingOffset
  )
  -- Right wing
  love.graphics.polygon("fill",
    8, -2 + wingOffset,
    12, -5 + wingOffset,
    10, 2 + wingOffset,
    6, 0 + wingOffset
  )

  -- Body
  love.graphics.setColor(bodyColor)
  love.graphics.ellipse("fill", 0, 0, 4, 6)

  -- Head
  love.graphics.setColor(bodyColor)
  love.graphics.ellipse("fill", 0, -4, 3, 3)

  -- Eyes (with glow effect)
  love.graphics.setColor(eyeColor)
  -- Main eyes
  love.graphics.rectangle("fill", -2, -5, 1, 1)
  love.graphics.rectangle("fill", 1, -5, 1, 1)

  -- Eye glow for better visibility
  love.graphics.setColor(1.0, 0.5, 0.5, 0.5) -- Semi-transparent red glow
  love.graphics.rectangle("fill", -2.5, -5.5, 2, 2)
  love.graphics.rectangle("fill", 0.5, -5.5, 2, 2)

  -- Ears (pointed)
  love.graphics.setColor(bodyColor)
  love.graphics.polygon("fill", -2, -7, -1, -6, -3, -6)
  love.graphics.polygon("fill", 2, -7, 1, -6, 3, -6)

  love.graphics.pop()

  -- Reset color to white for other drawing operations
  love.graphics.setColor(1, 1, 1, 1)
end

-- Draw all bats
function bats.drawBats(gameState)
  for _, bat in ipairs(gameState.bats) do
    bats.drawBat(bat)
  end
end

function bats.checkCollision(gameState)
  local player = gameState.player

  -- Collision with bats
  for _, bat in ipairs(gameState.bats) do
    if collision.checkCollision(player, bat) then
      audio.playDamage() -- Play damage sound for bat collision
      if playerDeath.killPlayerAtCenter(gameState, "bat") then
        return true      -- Only lose one life per frame
      end
    end
  end
  return false
end

return bats
