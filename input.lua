local particles = require("particles")
local audio = require("audio")

local input = {}

function input.handleKeyPressed(key, gameState, levels, gamestate)
  if gameState.currentState == "menu" then
    -- Menu navigation
    if key == "up" then
      gamestate.navigateMenuUp(gameState)
      audio.playMenuNavigate()
    elseif key == "down" then
      gamestate.navigateMenuDown(gameState)
      audio.playMenuNavigate()
    elseif key == "return" or key == "space" then
      audio.playMenuSelect()
      local action = gamestate.selectMenuOption(gameState)
      if action == "start_game" or action == "new_game" then
        -- Initialize the game when starting new game
        gameState.player = require("player").initializePlayer()
        gamestate.resetPlayerPositionSafe(gameState)
        levels.createLevel(gameState)
      elseif action == "resume_game" then
        -- Just resume the existing game state
        -- No need to reinitialize anything
      elseif action == "toggle_fullscreen" then
        gamestate.toggleFullscreen(gameState)
      elseif action == "quit" then
        love.event.quit()
      end
    elseif key == "escape" then
      love.event.quit()
    end
  elseif gameState.currentState == "playing" then
    if key == "escape" then
      -- Return to menu from game, but keep the game state
      gamestate.pauseToMenu(gameState)
    elseif key == "r" and (gameState.gameOver or gameState.won) then
      -- Restart game - go back to menu
      particles.clear() -- Clear all particles when restarting
      gamestate.reset(gameState)
      -- Don't initialize player and level here - wait for menu selection
    end
  end
end

return input
