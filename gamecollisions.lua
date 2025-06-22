local collision = require("collision")
local particles = require("particles")

local gamecollisions = {}

function gamecollisions.checkCollisions(gameState)
  local player = gameState.player
  local playerModule = require("player")

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
  end -- Collision with spikes (deadly trap)
  for _, spike in ipairs(gameState.spikes) do
    if collision.checkCollision(player, spike) and not gameState.invulnerable and not player.isWaitingToRespawn then
      -- Create both blood particles and death particles for visual effect
      particles.createBloodParticles(player.x + player.width / 2, player.y + player.height / 2)
      particles.createPlayerDeathParticles(player.x + player.width / 2, player.y + player.height / 2)

      gameState.lives = gameState.lives - 1
      if gameState.lives <= 0 then
        gameState.gameOver = true
      else
        playerModule.startDelayedRespawn(gameState)
      end
      break -- Only lose one life per frame
    end
  end

  -- Collision with enemies
  for _, enemy in ipairs(gameState.enemies) do
    if collision.checkCollision(player, enemy) and not gameState.invulnerable and not player.isWaitingToRespawn then
      -- Create both blood particles and death particles for visual effect
      particles.createBloodParticles(player.x + player.width / 2, player.y + player.height / 2)
      particles.createPlayerDeathParticles(player.x + player.width / 2, player.y + player.height / 2)

      gameState.lives = gameState.lives - 1
      if gameState.lives <= 0 then
        gameState.gameOver = true
      else
        playerModule.startDelayedRespawn(gameState)
      end
      break -- Only lose one life per frame
    end
  end

  -- Collision with bats
  for _, bat in ipairs(gameState.bats) do
    if collision.checkCollision(player, bat) and not gameState.invulnerable and not player.isWaitingToRespawn then
      -- Create both blood particles and death particles for visual effect
      particles.createBloodParticles(player.x + player.width / 2, player.y + player.height / 2)
      particles.createPlayerDeathParticles(player.x + player.width / 2, player.y + player.height / 2)

      gameState.lives = gameState.lives - 1
      if gameState.lives <= 0 then
        gameState.gameOver = true
      else
        playerModule.startDelayedRespawn(gameState)
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
