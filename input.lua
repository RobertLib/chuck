local input = {}
local particles = require("particles")

function input.handleKeyPressed(key, gameState, levels, gamestate)
  if key == "escape" then
    love.event.quit()
  elseif key == "r" and (gameState.gameOver or gameState.won) then
    -- Restart game
    particles.clear() -- Clear all particles when restarting
    gamestate.reset(gameState)
    gameState.player.x = 50
    gameState.player.y = 500
    gameState.player.velocityX = 0
    gameState.player.velocityY = 0
    gameState.player.animState = "idle"
    gameState.player.animTime = 0
    gameState.player.facing = 1
    levels.createLevel(gameState)
  end
end

return input
