local collision = require("collision")
local gamestate = require("gamestate")

local gamecollisions = {}

function gamecollisions.checkCollisions(gameState)
  local player = gameState.player

  -- Collision with collectibles
  for _, collectible in ipairs(gameState.collectibles) do
    if not collectible.collected and collision.checkCollision(player, collectible) then
      collectible.collected = true
      gameState.score = gameState.score + 100
    end
  end

  -- Collision with spikes (deadly trap)
  for _, spike in ipairs(gameState.spikes) do
    if collision.checkCollision(player, spike) and not gameState.invulnerable then
      gameState.lives = gameState.lives - 1
      if gameState.lives <= 0 then
        gameState.gameOver = true
      else
        gamestate.resetPlayerPosition(gameState)
        gameState.invulnerable = true
        gameState.invulnerabilityTimer = require("constants").INVULNERABILITY_DURATION
      end
      break -- Only lose one life per frame
    end
  end

  -- Collision with enemies
  for _, enemy in ipairs(gameState.enemies) do
    if collision.checkCollision(player, enemy) and not gameState.invulnerable then
      gameState.lives = gameState.lives - 1
      if gameState.lives <= 0 then
        gameState.gameOver = true
      else
        gamestate.resetPlayerPosition(gameState)
        gameState.invulnerable = true
        gameState.invulnerabilityTimer = require("constants").INVULNERABILITY_DURATION
      end
      break -- Only lose one life per frame
    end
  end
end

function gamecollisions.checkWinCondition(gameState, levelCount)
  local allCollectiblesCollected = true
  for _, collectible in ipairs(gameState.collectibles) do
    if not collectible.collected then
      allCollectiblesCollected = false
      break
    end
  end

  if allCollectiblesCollected and not gameState.showingLevelComplete then
    if gameState.level < levelCount then
      -- Start showing level complete message
      gameState.showingLevelComplete = true
      gameState.levelCompleteTimer = require("constants").LEVEL_COMPLETE_DISPLAY_TIME
    else
      -- Final victory
      gameState.won = true
    end
  end
end

return gamecollisions
