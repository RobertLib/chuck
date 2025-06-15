-- Centralized player death handling
local particles = require("particles")
local audio = require("audio")

local playerDeath = {}

-- Handle player death with consistent logic
function playerDeath.killPlayer(gameState, x, y, reason)
  local player = gameState.player

  -- Prevent multiple deaths
  if player.isWaitingToRespawn or gameState.invulnerable then
    return false
  end

  -- Create death effects
  particles.createBloodParticles(x, y)
  particles.createPlayerDeathParticles(x, y)

  -- Play death sound
  audio.playDeath()

  -- Handle lives and game over
  gameState.lives = gameState.lives - 1
  if gameState.lives <= 0 then
    gameState.gameOver = true
  else
    local playerModule = require("player")
    playerModule.startDelayedRespawn(gameState)
  end

  return true
end

-- Convenience function for player center position
function playerDeath.killPlayerAtCenter(gameState, reason)
  local player = gameState.player
  local centerX = player.x + player.width / 2
  local centerY = player.y + player.height / 2
  return playerDeath.killPlayer(gameState, centerX, centerY, reason)
end

return playerDeath
