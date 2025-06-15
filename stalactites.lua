local constants = require("constants")
local collision = require("collision")
local playerDeath = require("player_death")
local particles = require("particles")

local stalactites = {}

-- Stalactite colors
local STALACTITE_COLORS = {
  stone = { 0.6, 0.6, 0.7 },
  shadow = { 0.4, 0.4, 0.5 },
  highlight = { 0.8, 0.8, 0.9 }
}

-- Initialize a stalactite
function stalactites.init(x, y, width, height)
  return {
    x = x,
    y = y,
    width = width or constants.STALACTITE_WIDTH,
    height = height or constants.STALACTITE_HEIGHT,
    originalY = y,
    falling = false,
    fallen = false, -- New state: stalactite has fallen and is broken
    fallSpeed = 0,
    detectionRange = constants.STALACTITE_DETECTION_RANGE,
    hasBeenTriggered = false,
    debrisTimer = 0,   -- Timer for debris visibility
    fallDelayTimer = 0 -- Timer for delay before falling
  }
end

-- Check if there's a clear path between stalactite and player
local function hasClearPath(stalactite, player, gameState)
  local stalactiteBottom = stalactite.y + stalactite.height
  local playerTop = player.y

  -- Only check if player is below the stalactite
  if playerTop <= stalactiteBottom then
    return false
  end

  -- Check horizontal alignment (player must be roughly under stalactite)
  local stalactiteCenter = stalactite.x + stalactite.width / 2
  local playerCenter = player.x + player.width / 2

  if math.abs(stalactiteCenter - playerCenter) > stalactite.width then
    return false
  end

  -- Create a thin vertical rectangle to check for obstacles
  local rayWidth = 4
  local rayRect = {
    x = stalactiteCenter - rayWidth / 2,
    y = stalactiteBottom,
    width = rayWidth,
    height = playerTop - stalactiteBottom
  }

  -- Check collision with platforms
  for _, platform in ipairs(gameState.platforms) do
    if collision.checkCollision(rayRect, platform) then
      return false
    end
  end

  -- Check collision with moving platforms
  for _, platform in ipairs(gameState.movingPlatforms) do
    if collision.checkCollision(rayRect, platform) then
      return false
    end
  end

  -- Check collision with crumbling platforms
  for _, platform in ipairs(gameState.crumblingPlatforms) do
    if not platform.destroyed and collision.checkCollision(rayRect, platform) then
      return false
    end
  end

  -- Check collision with conveyor belts
  if gameState.conveyorBelts then
    for _, belt in ipairs(gameState.conveyorBelts) do
      if collision.checkCollision(rayRect, belt) then
        return false
      end
    end
  end

  return true
end

-- Update stalactites
function stalactites.update(gameState, dt)
  local player = gameState.player

  for _, stalactite in ipairs(gameState.stalactites) do
    if not stalactite.falling and not stalactite.hasBeenTriggered and not stalactite.fallen then
      -- Check if player is in detection range and has clear path
      local distance = math.abs(player.x + player.width / 2 - (stalactite.x + stalactite.width / 2))
      local verticalDistance = player.y - (stalactite.y + stalactite.height)

      if distance <= stalactite.width and
          verticalDistance > 0 and
          verticalDistance <= stalactite.detectionRange and
          hasClearPath(stalactite, player, gameState) then
        -- Start delay timer
        stalactite.hasBeenTriggered = true
        stalactite.fallDelayTimer = constants.STALACTITE_FALL_DELAY
      end
    elseif stalactite.hasBeenTriggered and not stalactite.falling and stalactite.fallDelayTimer > 0 then
      -- Count down delay timer
      stalactite.fallDelayTimer = stalactite.fallDelayTimer - dt
      if stalactite.fallDelayTimer <= 0 then
        -- Start falling after delay
        stalactite.falling = true
        stalactite.fallSpeed = constants.STALACTITE_FALL_SPEED
      end
    elseif stalactite.falling then
      -- Apply gravity and move stalactite down
      stalactite.fallSpeed = stalactite.fallSpeed + constants.STALACTITE_FALL_GRAVITY * dt
      stalactite.y = stalactite.y + stalactite.fallSpeed * dt

      -- Check if stalactite hit the ground or went off screen
      if stalactite.y > constants.SCREEN_HEIGHT + 50 then
        stalactite.falling = false
        stalactite.fallen = true
        stalactite.fallSpeed = 0
        stalactite.debrisTimer = constants.STALACTITE_DEBRIS_DURATION
        -- Don't reset position - stalactite remains off-screen as broken
      end

      -- Check collision with platforms (stalactite breaks on impact)
      for _, platform in ipairs(gameState.platforms) do
        if collision.checkCollision(stalactite, platform) then
          stalactite.falling = false
          stalactite.fallen = true
          stalactite.fallSpeed = 0
          stalactite.debrisTimer = constants.STALACTITE_DEBRIS_DURATION
          -- Position stalactite on top of platform as broken pieces
          stalactite.y = platform.y - stalactite.height

          -- Create impact particles
          particles.createImpactParticles(
            stalactite.x + stalactite.width / 2,
            platform.y,
            STALACTITE_COLORS.stone
          )
          break
        end
      end

      -- Check collision with moving platforms
      for _, platform in ipairs(gameState.movingPlatforms) do
        if collision.checkCollision(stalactite, platform) then
          stalactite.falling = false
          stalactite.fallen = true
          stalactite.fallSpeed = 0
          stalactite.debrisTimer = constants.STALACTITE_DEBRIS_DURATION
          -- Position stalactite on top of platform as broken pieces
          stalactite.y = platform.y - stalactite.height

          particles.createImpactParticles(
            stalactite.x + stalactite.width / 2,
            platform.y,
            STALACTITE_COLORS.stone
          )
          break
        end
      end
    elseif stalactite.fallen and stalactite.debrisTimer > 0 then
      -- Update debris timer
      stalactite.debrisTimer = stalactite.debrisTimer - dt
      -- Stalactite remains fallen permanently - no reset
    end
  end
end

-- Draw stalactites
function stalactites.draw(gameState)
  for _, stalactite in ipairs(gameState.stalactites) do
    local x = stalactite.x
    local y = stalactite.y
    local width = stalactite.width
    local height = stalactite.height

    if stalactite.fallen and stalactite.debrisTimer > 0 then
      -- Calculate fade alpha based on remaining time
      local fadeAlpha = stalactite.debrisTimer / constants.STALACTITE_DEBRIS_DURATION

      -- Draw as broken debris on the ground
      love.graphics.setColor(STALACTITE_COLORS.shadow[1], STALACTITE_COLORS.shadow[2], STALACTITE_COLORS.shadow[3],
        fadeAlpha)

      -- Draw scattered rock pieces
      for i = 1, 3 do
        local pieceX = x + (i - 1) * width / 3
        local pieceY = y + height - 4
        local pieceSize = 6 + i
        love.graphics.rectangle("fill", pieceX, pieceY, pieceSize, pieceSize)
      end

      -- Draw some smaller fragments
      love.graphics.setColor(STALACTITE_COLORS.stone[1], STALACTITE_COLORS.stone[2], STALACTITE_COLORS.stone[3],
        fadeAlpha)
      for i = 1, 5 do
        local fragX = x + i * width / 6
        local fragY = y + height - 2
        local fragSize = 2
        love.graphics.rectangle("fill", fragX, fragY, fragSize, fragSize)
      end
    elseif not stalactite.fallen then
      -- Draw hanging or falling stalactite (tapered cone shape)
      love.graphics.setColor(STALACTITE_COLORS.stone)

      -- If in delay phase, add subtle shaking effect
      local shakeX = 0
      if stalactite.hasBeenTriggered and not stalactite.falling and stalactite.fallDelayTimer > 0 then
        -- Subtle shake effect during delay
        local shakeIntensity = 1 - (stalactite.fallDelayTimer / constants.STALACTITE_FALL_DELAY)
        shakeX = (math.random() - 0.5) * 2 * shakeIntensity
      end

      -- Draw as a series of horizontal rectangles that get narrower towards the bottom
      local segments = 8
      for i = 0, segments - 1 do
        local segmentHeight = height / segments
        local segmentY = y + i * segmentHeight
        local tapering = (segments - i) / segments -- 1.0 at top, approaches 0 at bottom
        local segmentWidth = width * tapering
        local segmentX = x + (width - segmentWidth) / 2 + shakeX

        love.graphics.rectangle("fill", segmentX, segmentY, segmentWidth, segmentHeight + 1)
      end

      -- Add shadow on the left side
      love.graphics.setColor(STALACTITE_COLORS.shadow)
      for i = 0, segments - 1 do
        local segmentHeight = height / segments
        local segmentY = y + i * segmentHeight
        local tapering = (segments - i) / segments
        local segmentWidth = width * tapering
        local segmentX = x + (width - segmentWidth) / 2 + shakeX
        local shadowWidth = math.max(1, segmentWidth * 0.3)

        love.graphics.rectangle("fill", segmentX, segmentY, shadowWidth, segmentHeight + 1)
      end

      -- Add highlight on the right side
      love.graphics.setColor(STALACTITE_COLORS.highlight)
      for i = 0, segments - 1 do
        local segmentHeight = height / segments
        local segmentY = y + i * segmentHeight
        local tapering = (segments - i) / segments
        local segmentWidth = width * tapering
        local segmentX = x + (width - segmentWidth) / 2 + shakeX
        local highlightWidth = math.max(1, segmentWidth * 0.2)

        love.graphics.rectangle("fill", segmentX + segmentWidth - highlightWidth, segmentY, highlightWidth,
          segmentHeight + 1)
      end

      -- Draw pointed tip
      love.graphics.setColor(STALACTITE_COLORS.stone)
      local tipX = x + width / 2 + shakeX
      local tipY = y + height
      local tipSize = 3

      love.graphics.polygon("fill",
        tipX, tipY + tipSize, -- Bottom point
        tipX - tipSize, tipY, -- Top left
        tipX + tipSize, tipY  -- Top right
      )
    end
  end
end

-- Check collision with player
function stalactites.checkCollision(gameState)
  local player = gameState.player

  for _, stalactite in ipairs(gameState.stalactites) do
    if stalactite.falling and collision.checkCollision(player, stalactite) then
      if playerDeath.killPlayerAtCenter(gameState, "stalactite") then
        -- Create impact particles
        particles.createImpactParticles(
          player.x + player.width / 2,
          player.y + player.height / 2,
          STALACTITE_COLORS.stone
        )

        -- Stalactite becomes fallen debris - doesn't reset
        stalactite.falling = false
        stalactite.fallen = true
        stalactite.fallSpeed = 0
        stalactite.debrisTimer = constants.STALACTITE_DEBRIS_DURATION
        -- Keep current position as broken debris

        return true
      end
    end
  end
  return false
end

return stalactites
