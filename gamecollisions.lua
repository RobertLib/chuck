local collision = require("collision")
local particles = require("particles")
local playerDeath = require("player_death")

local gamecollisions = {}

function gamecollisions.checkCollisions(gameState)
  local player = gameState.player

  -- Collision with collectibles
  for _, collectible in ipairs(gameState.collectibles) do
    if not collectible.collected and collision.checkCollision(player, collectible) then
      -- Create golden sparkle particles when collecting item
      particles.createImpactParticles(
        collectible.x + collectible.width / 2,
        collectible.y + collectible.height / 2,
        { 1.0, 0.8, 0.2, 1.0 } -- Golden color
      )

      collectible.collected = true
      -- Start pickup animation
      collectible.pickupTime = 0
      collectible.isPickingUp = true
      gameState.score = gameState.score + 100
    end
  end

  -- Collision with spikes (deadly trap)
  for _, spike in ipairs(gameState.spikes) do
    if collision.checkCollision(player, spike) then
      if playerDeath.killPlayerAtCenter(gameState, "spike") then
        break -- Only lose one life per frame
      end
    end
  end

  -- Collision with enemies
  for _, enemy in ipairs(gameState.enemies) do
    if collision.checkCollision(player, enemy) then
      if playerDeath.killPlayerAtCenter(gameState, "enemy") then
        break -- Only lose one life per frame
      end
    end
  end

  -- Collision with bats
  for _, bat in ipairs(gameState.bats) do
    if collision.checkCollision(player, bat) then
      if playerDeath.killPlayerAtCenter(gameState, "bat") then
        break -- Only lose one life per frame
      end
    end
  end

  -- Collision with fireballs
  for i = #gameState.fireballs, 1, -1 do
    local fireball = gameState.fireballs[i]
    if collision.checkCollision(player, fireball) then
      if playerDeath.killPlayerAtCenter(gameState, "fireball") then
        -- Create explosion particles at fireball location
        particles.createImpactParticles(
          fireball.x + fireball.width / 2,
          fireball.y + fireball.height / 2,
          { 1.0, 0.5, 0.1, 1.0 } -- Orange explosion color
        )

        -- Remove the fireball after hit
        table.remove(gameState.fireballs, i)
        break -- Only lose one life per frame
      end
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
