local constants = require("constants")
local background = require("background")
local platforms = require("platforms")
local ladders = require("ladders")
local collectibles = require("collectibles")
local crates = require("crates")
local spikes = require("spikes")
local enemies = require("enemies")
local levels = require("levels")
local particles = require("particles")
local gamestate = require("gamestate")
local player = require("player")

local transition = {}

-- Transition state
local transitionState = {
  active = false,
  timer = 0,
  duration = 1.5,         -- Duration of transition in seconds
  currentLevelOffset = 0, -- X offset for current level
  nextLevelOffset = 0,    -- X offset for next level
  currentLevel = nil,     -- Current level data to render during transition
  nextLevel = nil,        -- Next level data to render during transition
  targetLevel = nil       -- Target level number
}

-- Start level transition
function transition.startTransition(gameState, targetLevel)
  if transitionState.active then return false end

  -- Clear any existing particles when starting transition
  particles.clear()

  -- Store current level state for rendering during transition
  transitionState.currentLevel = {
    platforms = {},
    ladders = {},
    collectibles = {},
    lifePowerups = {},
    enemies = {},
    crates = {},
    spikes = {},
    player = {
      x = gameState.player.x,
      y = gameState.player.y,
      width = gameState.player.width,
      height = gameState.player.height,
      animState = gameState.player.animState,
      animTime = gameState.player.animTime,
      facing = gameState.player.facing,
      velocityX = gameState.player.velocityX or 0,
      velocityY = gameState.player.velocityY or 0,
      onGround = gameState.player.onGround or true,
      onLadder = gameState.player.onLadder or false
    }
  }

  -- Copy current level objects
  for _, platform in ipairs(gameState.platforms) do
    table.insert(transitionState.currentLevel.platforms, {
      x = platform.x, y = platform.y, width = platform.width, height = platform.height
    })
  end

  for _, ladder in ipairs(gameState.ladders) do
    table.insert(transitionState.currentLevel.ladders, {
      x = ladder.x, y = ladder.y, width = ladder.width, height = ladder.height
    })
  end

  for _, collectible in ipairs(gameState.collectibles) do
    table.insert(transitionState.currentLevel.collectibles, {
      x = collectible.x,
      y = collectible.y,
      width = collectible.width,
      height = collectible.height,
      collected = collectible.collected,
      animTime = collectible.animTime
    })
  end

  for _, lifePowerup in ipairs(gameState.lifePowerups or {}) do
    table.insert(transitionState.currentLevel.lifePowerups, {
      x = lifePowerup.x,
      y = lifePowerup.y,
      width = lifePowerup.width,
      height = lifePowerup.height,
      collected = lifePowerup.collected,
      animTime = lifePowerup.animTime
    })
  end

  for _, enemy in ipairs(gameState.enemies) do
    table.insert(transitionState.currentLevel.enemies, {
      x = enemy.x,
      y = enemy.y,
      width = enemy.width,
      height = enemy.height,
      velocityX = 0 -- Freeze enemies during transition
    })
  end

  for _, crate in ipairs(gameState.crates) do
    table.insert(transitionState.currentLevel.crates, {
      x = crate.x, y = crate.y, width = crate.width, height = crate.height
    })
  end

  for _, spike in ipairs(gameState.spikes) do
    table.insert(transitionState.currentLevel.spikes, {
      x = spike.x, y = spike.y, width = spike.width, height = spike.height
    })
  end

  -- Set up transition
  transitionState.active = true
  transitionState.timer = 0
  transitionState.currentLevelOffset = 0
  transitionState.nextLevelOffset = constants.SCREEN_WIDTH
  transitionState.targetLevel = targetLevel

  -- Create next level data (but don't update gameState yet)
  local tempGameState = {
    platforms = {},
    ladders = {},
    collectibles = {},
    lifePowerups = {},
    enemies = {},
    crates = {},
    spikes = {},
    level = targetLevel,
    player = player.initializePlayer() -- Add dummy player for spawn position checking
  }

  levels.createLevel(tempGameState)

  -- Store next level data
  transitionState.nextLevel = {
    platforms = tempGameState.platforms,
    ladders = tempGameState.ladders,
    collectibles = tempGameState.collectibles,
    lifePowerups = tempGameState.lifePowerups,
    enemies = tempGameState.enemies,
    crates = tempGameState.crates,
    spikes = tempGameState.spikes
  }

  return true
end

-- Update transition
function transition.updateTransition(dt, gameState)
  if not transitionState.active then return false end

  transitionState.timer = transitionState.timer + dt

  -- Calculate transition progress (0 to 1)
  local progress = math.min(transitionState.timer / transitionState.duration, 1)

  -- Use smooth easing function (ease-out)
  local easedProgress = 1 - (1 - progress) * (1 - progress) * (1 - progress)

  -- Update level offsets
  transitionState.currentLevelOffset = -easedProgress * constants.SCREEN_WIDTH
  transitionState.nextLevelOffset = constants.SCREEN_WIDTH - easedProgress * constants.SCREEN_WIDTH

  -- Check if transition is complete
  if progress >= 1 then
    -- Transition finished - update game state with new level
    gameState.level = transitionState.targetLevel
    gameState.showingLevelComplete = false

    levels.createLevel(gameState)

    -- Reset player position safely
    gamestate.resetPlayerPositionSafe(gameState)

    -- Reset transition state
    transitionState.active = false
    transitionState.timer = 0
    transitionState.currentLevel = nil
    transitionState.nextLevel = nil
    transitionState.targetLevel = nil

    return false -- Transition complete
  end

  return true -- Transition still active
end

-- Skip transition (called when player presses a key)
function transition.skipTransition(gameState)
  if not transitionState.active then return false end

  -- Complete transition immediately
  gameState.level = transitionState.targetLevel
  gameState.showingLevelComplete = false

  levels.createLevel(gameState)

  -- Reset player position safely
  gamestate.resetPlayerPositionSafe(gameState)

  -- Reset transition state
  transitionState.active = false
  transitionState.timer = 0
  transitionState.currentLevel = nil
  transitionState.nextLevel = nil
  transitionState.targetLevel = nil

  return true
end

-- Draw transition
function transition.drawTransition(gameState)
  if not transitionState.active then return false end

  -- Set up clipping to screen bounds
  love.graphics.push()
  love.graphics.setScissor(0, 0, constants.SCREEN_WIDTH, constants.SCREEN_HEIGHT)

  -- Draw current level (moving left)
  love.graphics.push()
  love.graphics.translate(transitionState.currentLevelOffset, 0)

  -- Draw background
  background.draw(gameState)

  -- Draw current level objects
  transition.drawLevelObjects(transitionState.currentLevel, false) -- false = current level

  love.graphics.pop()

  -- Draw next level (moving in from right)
  love.graphics.push()
  love.graphics.translate(transitionState.nextLevelOffset, 0)

  -- Draw background
  background.draw(gameState)

  -- Draw next level objects
  transition.drawLevelObjects(transitionState.nextLevel, true) -- true = next level

  love.graphics.pop()

  love.graphics.setScissor()
  love.graphics.pop()

  return true
end

-- Helper function to draw level objects
function transition.drawLevelObjects(levelData, isNextLevel)
  -- Create temporary gameState for rendering
  local tempGameState = {
    platforms = levelData.platforms or {},
    ladders = levelData.ladders or {},
    collectibles = levelData.collectibles or {},
    lifePowerups = levelData.lifePowerups or {},
    enemies = levelData.enemies or {},
    crates = levelData.crates or {},
    spikes = levelData.spikes or {}
  }

  -- Draw platforms
  platforms.draw(tempGameState)

  -- Draw ladders
  ladders.draw(tempGameState)

  -- Draw collectibles
  for _, collectible in ipairs(tempGameState.collectibles) do
    if not collectible.collected then
      collectibles.drawCollectibleItem(collectible)
    elseif collectible.isPickingUp then
      collectibles.drawPickupAnimation(collectible)
    end
  end

  -- Draw life power-ups
  for _, lifePowerup in ipairs(tempGameState.lifePowerups) do
    if not lifePowerup.collected then
      collectibles.drawLifePowerupItem(lifePowerup)
    elseif lifePowerup.isPickingUp then
      collectibles.drawPickupAnimation(lifePowerup)
    end
  end

  -- Draw crates
  crates.drawCrates(tempGameState)

  -- Draw spikes
  spikes.draw(tempGameState)

  -- Draw enemies
  for _, enemy in ipairs(tempGameState.enemies) do
    enemies.drawEnemy(enemy)
  end

  -- Draw player only for current level
  if not isNextLevel and levelData.player then
    -- Create temporary gameState with player for rendering
    local playerGameState = {
      player = {
        x = levelData.player.x,
        y = levelData.player.y,
        width = levelData.player.width,
        height = levelData.player.height,
        animState = levelData.player.animState,
        animTime = levelData.player.animTime,
        facing = levelData.player.facing,
        velocityX = 0, -- Add missing velocity values
        velocityY = 0,
        onGround = true,
        onLadder = false
      },
      invulnerable = false,
      invulnerabilityTimer = 0
    }
    player.drawPlayer(playerGameState)
  end
end

-- Check if transition is active
function transition.isActive()
  return transitionState.active
end

-- Get transition progress (0 to 1)
function transition.getProgress()
  if not transitionState.active then return 0 end
  return math.min(transitionState.timer / transitionState.duration, 1)
end

return transition
